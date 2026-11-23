#include "m5_pocket/bridge.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "m5_pocket/app.h"
#include "sdkconfig.h"
#include <stdlib.h>
#include <string.h>

/* One named board value. Storage for text and arrays grows on demand and is
 * kept for the program's lifetime; `dirty` marks a value the app host has
 * not taken since its last change. */
typedef struct {
  char name[M5_POCKET_BRIDGE_NAME_MAX + 1];
  uint8_t kind;
  bool dirty;
  double number;
  char *text;
  size_t text_length, text_capacity;
  float *array;
  size_t array_length, array_capacity;
} slot_t;

typedef struct {
  char name[M5_POCKET_BRIDGE_NAME_MAX + 1];
  double value;
} event_t;

static slot_t slots[CONFIG_M5_POCKET_BRIDGE_SLOTS];
static size_t slot_count;
static size_t next_slot; /* consumer cursor */
static event_t events[CONFIG_M5_POCKET_BRIDGE_EVENTS];
static size_t event_head, event_count;
static m5_pocket_bridge_stats stats;
static m5_pocket_request_fn request_handler;
static void *request_context;

/* The consumer's copy of the value it is handling, so that producers can
 * keep publishing while the app host reads it without the lock. */
static char taken_name[M5_POCKET_BRIDGE_NAME_MAX + 1];
static char *taken_text;
static size_t taken_text_capacity;
static float *taken_array;
static size_t taken_array_capacity;

static StaticSemaphore_t lock_storage;
static SemaphoreHandle_t lock_handle;
static portMUX_TYPE lock_init = portMUX_INITIALIZER_UNLOCKED;

static void lock(void) {
  if (!lock_handle) {
    portENTER_CRITICAL(&lock_init);
    if (!lock_handle)
      lock_handle = xSemaphoreCreateMutexStatic(&lock_storage);
    portEXIT_CRITICAL(&lock_init);
  }
  xSemaphoreTake(lock_handle, portMAX_DELAY);
}

static void unlock(void) { xSemaphoreGive(lock_handle); }

static bool valid_name(const char *name) {
  if (!name || !name[0])
    return false;
  return strnlen(name, M5_POCKET_BRIDGE_NAME_MAX + 1) <=
         M5_POCKET_BRIDGE_NAME_MAX;
}

/* Find or create a slot; called with the lock held. */
static slot_t *slot_for(const char *name) {
  for (size_t i = 0; i < slot_count; i++)
    if (strcmp(slots[i].name, name) == 0)
      return &slots[i];
  if (slot_count == CONFIG_M5_POCKET_BRIDGE_SLOTS) {
    stats.values_refused++;
    return NULL;
  }
  slot_t *slot = &slots[slot_count++];
  memset(slot, 0, sizeof(*slot));
  strcpy(slot->name, name);
  return slot;
}

static bool reserve(void **storage, size_t *capacity, size_t bytes) {
  if (bytes <= *capacity)
    return true;
  void *grown = realloc(*storage, bytes);
  if (!grown)
    return false;
  *storage = grown;
  *capacity = bytes;
  return true;
}

esp_err_t m5_pocket_bridge_set_number(const char *name, double value) {
  if (!valid_name(name))
    return ESP_ERR_INVALID_ARG;
  lock();
  slot_t *slot = slot_for(name);
  if (slot) {
    if (slot->kind != M5_POCKET_VALUE_NUMBER || slot->number != value)
      slot->dirty = true;
    slot->kind = M5_POCKET_VALUE_NUMBER;
    slot->number = value;
  }
  unlock();
  return slot ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t m5_pocket_bridge_set_bool(const char *name, bool value) {
  return m5_pocket_bridge_set_number(name, value ? 1.0 : 0.0);
}

esp_err_t m5_pocket_bridge_set_text(const char *name, const char *text) {
  if (!valid_name(name) || !text)
    return ESP_ERR_INVALID_ARG;
  size_t length = strnlen(text, CONFIG_M5_POCKET_BRIDGE_TEXT_MAX);
  /* Cut at a UTF-8 character boundary. */
  while (length > 0 && length == CONFIG_M5_POCKET_BRIDGE_TEXT_MAX &&
         ((unsigned char)text[length] & 0xC0) == 0x80)
    length--;
  esp_err_t result = ESP_OK;
  lock();
  slot_t *slot = slot_for(name);
  if (!slot) {
    result = ESP_ERR_NO_MEM;
  } else if (!reserve((void **)&slot->text, &slot->text_capacity, length + 1)) {
    stats.values_refused++;
    result = ESP_ERR_NO_MEM;
  } else {
    if (slot->kind != M5_POCKET_VALUE_TEXT || slot->text_length != length ||
        memcmp(slot->text, text, length) != 0)
      slot->dirty = true;
    slot->kind = M5_POCKET_VALUE_TEXT;
    memcpy(slot->text, text, length);
    slot->text[length] = 0;
    slot->text_length = length;
  }
  unlock();
  return result;
}

esp_err_t m5_pocket_bridge_set_array(const char *name, const float *values,
                                     size_t count) {
  if (!valid_name(name) || (count && !values))
    return ESP_ERR_INVALID_ARG;
  if (count > CONFIG_M5_POCKET_BRIDGE_ARRAY_MAX)
    count = CONFIG_M5_POCKET_BRIDGE_ARRAY_MAX;
  esp_err_t result = ESP_OK;
  lock();
  slot_t *slot = slot_for(name);
  if (!slot) {
    result = ESP_ERR_NO_MEM;
  } else if (!reserve((void **)&slot->array, &slot->array_capacity,
                      (count ? count : 1) * sizeof(float))) {
    stats.values_refused++;
    result = ESP_ERR_NO_MEM;
  } else {
    /* Arrays carry sampled data (spectra, waveforms): every publication is
     * delivered, equal or not. */
    slot->dirty = true;
    slot->kind = M5_POCKET_VALUE_ARRAY;
    if (count)
      memcpy(slot->array, values, count * sizeof(float));
    slot->array_length = count;
  }
  unlock();
  return result;
}

esp_err_t m5_pocket_bridge_post_event(const char *name, double value) {
  if (!valid_name(name))
    return ESP_ERR_INVALID_ARG;
  esp_err_t result = ESP_OK;
  lock();
  if (event_count == CONFIG_M5_POCKET_BRIDGE_EVENTS) {
    stats.events_refused++;
    result = ESP_ERR_NO_MEM;
  } else {
    event_t *event =
        &events[(event_head + event_count) % CONFIG_M5_POCKET_BRIDGE_EVENTS];
    strcpy(event->name, name);
    event->value = value;
    event_count++;
  }
  unlock();
  return result;
}

static void fill_name(m5_pocket_value *out, const char *name) {
  strcpy(taken_name, name);
  out->name = taken_name;
  out->name_length = strlen(taken_name);
}

bool m5_pocket_bridge_next_value(m5_pocket_value *out) {
  if (!out || out->struct_size < sizeof(*out))
    return false;
  bool found = false;
  lock();
  for (size_t scanned = 0; scanned < slot_count && !found; scanned++) {
    slot_t *slot = &slots[next_slot];
    next_slot = (next_slot + 1) % slot_count;
    if (!slot->dirty)
      continue;
    if (slot->kind == M5_POCKET_VALUE_TEXT &&
        !reserve((void **)&taken_text, &taken_text_capacity,
                 slot->text_length + 1))
      continue; /* stays dirty; retried next frame */
    if (slot->kind == M5_POCKET_VALUE_ARRAY &&
        !reserve((void **)&taken_array, &taken_array_capacity,
                 (slot->array_length ? slot->array_length : 1) * sizeof(float)))
      continue;
    slot->dirty = false;
    found = true;
    *out = (m5_pocket_value){.struct_size = sizeof(*out), .kind = slot->kind};
    fill_name(out, slot->name);
    switch (slot->kind) {
    case M5_POCKET_VALUE_TEXT:
      memcpy(taken_text, slot->text, slot->text_length + 1);
      out->text = taken_text;
      out->text_length = slot->text_length;
      break;
    case M5_POCKET_VALUE_ARRAY:
      if (slot->array_length)
        memcpy(taken_array, slot->array, slot->array_length * sizeof(float));
      out->array = taken_array;
      out->array_length = slot->array_length;
      break;
    default:
      out->number = slot->number;
      break;
    }
  }
  unlock();
  return found;
}

bool m5_pocket_bridge_next_event(m5_pocket_value *out) {
  if (!out || out->struct_size < sizeof(*out))
    return false;
  bool found = false;
  lock();
  if (event_count) {
    const event_t *event = &events[event_head];
    *out = (m5_pocket_value){.struct_size = sizeof(*out),
                             .kind = M5_POCKET_VALUE_NUMBER,
                             .number = event->value};
    fill_name(out, event->name);
    event_head = (event_head + 1) % CONFIG_M5_POCKET_BRIDGE_EVENTS;
    event_count--;
    found = true;
  }
  unlock();
  return found;
}

void m5_pocket_bridge_set_request_handler(m5_pocket_request_fn handler,
                                          void *context) {
  lock();
  request_handler = handler;
  request_context = context;
  unlock();
}

typedef struct {
  m5_pocket_request_fn handler;
  void *context;
  const m5_pocket_request *request;
} request_call;

static void call_handler(void *argument) {
  const request_call *call = argument;
  call->handler(call->context, call->request);
}

static bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

esp_err_t m5_pocket_bridge_dispatch_request(const char *text, size_t length) {
  if (!text || !length)
    return ESP_ERR_INVALID_ARG;
  lock();
  m5_pocket_request_fn handler = request_handler;
  void *context = request_context;
  if (!handler)
    stats.requests_unhandled++;
  unlock();
  if (!handler)
    return ESP_ERR_INVALID_STATE;
  m5_pocket_request request = {
      .struct_size = sizeof(request), .text = text, .text_length = length};
  size_t at = 0;
  while (at < length && is_space(text[at]))
    at++;
  request.name = text + at;
  while (at < length && !is_space(text[at]))
    at++;
  request.name_length = (size_t)(text + at - request.name);
  char word[32];
  while (at < length && request.argc < M5_POCKET_REQUEST_ARGS_MAX) {
    while (at < length && is_space(text[at]))
      at++;
    size_t start = at;
    while (at < length && !is_space(text[at]))
      at++;
    size_t size = at - start;
    if (!size || size >= sizeof(word))
      continue;
    memcpy(word, text + start, size);
    word[size] = 0;
    char *end = NULL;
    double value = strtod(word, &end);
    if (end == word + size)
      request.args[request.argc++] = value;
  }
  /* On the helper task when this task's stack is in PSRAM: a request
   * handler may write flash. */
  request_call call = {handler, context, &request};
  return m5_pocket_app_run_internal(call_handler, &call);
}

esp_err_t m5_pocket_bridge_get_stats(m5_pocket_bridge_stats *out) {
  if (!out || out->struct_size < sizeof(*out))
    return ESP_ERR_INVALID_ARG;
  lock();
  *out = stats;
  unlock();
  out->struct_size = sizeof(*out);
  return ESP_OK;
}
