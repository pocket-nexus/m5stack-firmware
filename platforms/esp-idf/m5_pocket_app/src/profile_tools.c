/* Tools of the frame profiler: scripted input and a program counter
 * sampler, driven by lines on the USB Serial/JTAG port.
 *
 *   t <x> <y>                    contact down at a logical position, held
 *   u                            contact up
 *   p <x> <y> [frames]           tap: down for `frames` frames (default 3)
 *   w <x0> <y0> <x1> <y1> <frames>   swipe over `frames` frames, then up
 *   k <mask> [frames]            hold POCKET_BTN_* bits (hex); 0 frames = hold
 *   r <request>                  dispatch a request to the program's handler
 *                                from the app task, as the application does
 *   c <ms>                       sample the app task's PC for `ms`, then dump
 *
 * The sampler is a 997 us timer whose interrupt runs on the app task's core
 * and reads the interrupted PC and return address from the frame the
 * interrupt entry saved in the task's TCB. The dump lists every distinct
 * (stage, pc, return address) with its count; resolve the addresses with
 * addr2line on the firmware ELF. */
#include "profile.h"

#if CONFIG_M5_POCKET_APP_PROFILE

#include "driver/gptimer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "hal/usb_serial_jtag_ll.h"
#include "m5_pocket/bridge.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if CONFIG_IDF_TARGET_ARCH_XTENSA
#include "xtensa_context.h"
#else
#include "riscv/rvruntime-frames.h"
#endif

static const char *TAG = "m5_prof";

#define SAMPLE_PERIOD_US 997
#define SAMPLE_CAPACITY 20000u
/* Stage recorded for a sample taken on the app task outside a tick. */
#define STAGE_OUTSIDE M5_POCKET_STAGE_COUNT

/* -------------------------------- input ---------------------------------- */

typedef struct {
  char kind; /* t, u, p, w, k, r */
  float x0, y0, x1, y1;
  uint32_t frames, buttons;
  char text[72]; /* r */
} input_command;

static QueueHandle_t commands;

/* Script state, read and written on the app task. */
static struct {
  bool down;
  float x, y, x0, y0, x1, y1;
  uint32_t step, steps;
  uint32_t buttons, button_frames;
} script;

void m5_pocket_profile_input(uint32_t *buttons, bool *down, float *x,
                             float *y) {
  if (!commands)
    return;
  input_command command;
  while (xQueueReceive(commands, &command, 0) == pdTRUE) {
    switch (command.kind) {
    case 't':
      script.down = true;
      script.steps = 0;
      script.x = command.x0;
      script.y = command.y0;
      break;
    case 'u':
      script.down = false;
      script.steps = 0;
      break;
    case 'p':
    case 'w':
      script.down = true;
      script.x0 = command.x0;
      script.y0 = command.y0;
      script.x1 = command.x1;
      script.y1 = command.y1;
      script.step = 0;
      script.steps = command.frames ? command.frames : 1;
      break;
    case 'k':
      script.buttons = command.buttons;
      script.button_frames = command.frames;
      break;
    case 'r':
      (void)m5_pocket_bridge_dispatch_request(command.text,
                                              strlen(command.text));
      break;
    default:
      break;
    }
  }
  if (script.down && script.steps) {
    if (script.step > script.steps) {
      script.down = false;
      script.steps = 0;
    } else {
      const float t = (float)script.step / (float)script.steps;
      script.x = script.x0 + (script.x1 - script.x0) * t;
      script.y = script.y0 + (script.y1 - script.y0) * t;
      script.step++;
    }
  }
  if (script.down) {
    *down = true;
    *x = script.x;
    *y = script.y;
  }
  *buttons |= script.buttons;
  if (script.buttons && script.button_frames && --script.button_frames == 0)
    script.buttons = 0;
}

uint32_t m5_pocket_profile_buttons(void) { return script.buttons; }

/* ------------------------------- sampler --------------------------------- */

typedef struct {
  uint32_t pc, ra;
  uint32_t stage;
} sample;

static struct {
  gptimer_handle_t timer;
  TaskHandle_t app_task;
  sample *samples;
  volatile uint32_t count, other, dropped;
  volatile bool capturing;
} sampler;

static bool on_sample(gptimer_handle_t timer,
                      const gptimer_alarm_event_data_t *event, void *context) {
  (void)timer;
  (void)event;
  (void)context;
  if (!sampler.capturing)
    return false;
  const TaskHandle_t task = xTaskGetCurrentTaskHandle();
  if (task != sampler.app_task) {
    sampler.other++;
    return false;
  }
  const uint32_t n = sampler.count;
  if (n >= SAMPLE_CAPACITY) {
    sampler.dropped++;
    return false;
  }
  /* pxTopOfStack, the first TCB member, is the frame saved at the entry of
   * the outermost interrupt on this core. */
#if CONFIG_IDF_TARGET_ARCH_XTENSA
  const XtExcFrame *frame = *(const XtExcFrame *const *)task;
  /* The top two bits of a windowed return address hold the call size. */
  sampler.samples[n] =
      (sample){.pc = (uint32_t)frame->pc,
               .ra = ((uint32_t)frame->a0 & 0x3fffffffu) | 0x40000000u,
               .stage = m5_pocket_profile_current()};
#else
  const RvExcFrame *frame = *(const RvExcFrame *const *)task;
  sampler.samples[n] = (sample){.pc = (uint32_t)frame->mepc,
                                .ra = (uint32_t)frame->ra,
                                .stage = m5_pocket_profile_current()};
#endif
  sampler.count = n + 1;
  return false;
}

void m5_pocket_profile_sampler_attach(void) {
  /* On the app task: the timer interrupt is allocated on this core. */
  sampler.app_task = xTaskGetCurrentTaskHandle();
  const gptimer_config_t config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000,
  };
  const gptimer_alarm_config_t alarm = {
      .alarm_count = SAMPLE_PERIOD_US,
      .reload_count = 0,
      .flags.auto_reload_on_alarm = true,
  };
  const gptimer_event_callbacks_t callbacks = {.on_alarm = on_sample};
  if (gptimer_new_timer(&config, &sampler.timer) != ESP_OK ||
      gptimer_set_alarm_action(sampler.timer, &alarm) != ESP_OK ||
      gptimer_register_event_callbacks(sampler.timer, &callbacks, NULL) !=
          ESP_OK ||
      gptimer_enable(sampler.timer) != ESP_OK) {
    ESP_LOGE(TAG, "no sampler timer");
    sampler.timer = NULL;
  }
}

static int compare_samples(const void *left, const void *right) {
  const sample *a = left, *b = right;
  if (a->stage != b->stage)
    return a->stage < b->stage ? -1 : 1;
  if (a->pc != b->pc)
    return a->pc < b->pc ? -1 : 1;
  if (a->ra != b->ra)
    return a->ra < b->ra ? -1 : 1;
  return 0;
}

/* Tick count at which the running capture ends; 0 while idle. */
static TickType_t capture_end;

static void capture_begin(uint32_t ms) {
  if (capture_end)
    return;
  if (!sampler.timer) {
    ESP_LOGE(TAG, "no sampler timer");
    return;
  }
  if (!sampler.samples)
    sampler.samples = heap_caps_malloc(SAMPLE_CAPACITY * sizeof(sample),
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!sampler.samples)
    sampler.samples = heap_caps_malloc(SAMPLE_CAPACITY * sizeof(sample),
                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!sampler.samples) {
    ESP_LOGE(TAG, "no sample memory");
    return;
  }
  sampler.count = sampler.other = sampler.dropped = 0;
  sampler.capturing = true;
  gptimer_start(sampler.timer);
  capture_end = xTaskGetTickCount() + pdMS_TO_TICKS(ms);
  if (!capture_end)
    capture_end = 1;
}

static void capture_finish(void) {
  capture_end = 0;
  sampler.capturing = false;
  gptimer_stop(sampler.timer);
  const uint32_t count = sampler.count;
  qsort(sampler.samples, count, sizeof(sample), compare_samples);
  printf("m5_prof: C begin samples=%" PRIu32 " other=%" PRIu32
         " dropped=%" PRIu32 " period_us=%d\n",
         count, sampler.other, sampler.dropped, SAMPLE_PERIOD_US);
  for (uint32_t i = 0; i < count;) {
    uint32_t j = i + 1;
    while (j < count &&
           compare_samples(&sampler.samples[i], &sampler.samples[j]) == 0)
      j++;
    printf("Z %" PRIu32 " %08" PRIx32 " %08" PRIx32 " %" PRIu32 "\n",
           sampler.samples[i].stage, sampler.samples[i].pc,
           sampler.samples[i].ra, j - i);
    i = j;
  }
  printf("m5_prof: C end\n");
}

/* ------------------------------- console --------------------------------- */

static void run_line(char *line) {
  input_command command = {.kind = line[0]};
  float a = 0, b = 0, c = 0, d = 0;
  unsigned n = 0, mask = 0;
  switch (line[0]) {
  case 't':
    if (sscanf(line + 1, "%f %f", &a, &b) != 2)
      return;
    command.x0 = a;
    command.y0 = b;
    break;
  case 'u':
    break;
  case 'p':
    n = 3;
    if (sscanf(line + 1, "%f %f %u", &a, &b, &n) < 2)
      return;
    command.x0 = command.x1 = a;
    command.y0 = command.y1 = b;
    command.frames = n;
    break;
  case 'w':
    if (sscanf(line + 1, "%f %f %f %f %u", &a, &b, &c, &d, &n) != 5)
      return;
    command.x0 = a;
    command.y0 = b;
    command.x1 = c;
    command.y1 = d;
    command.frames = n;
    break;
  case 'k':
    if (sscanf(line + 1, "%x %u", &mask, &n) < 1)
      return;
    command.buttons = mask;
    command.frames = n;
    break;
  case 'r': {
    const char *text = line + 1;
    while (*text == ' ')
      text++;
    if (!*text)
      return;
    strlcpy(command.text, text, sizeof(command.text));
    break;
  }
  case 'c':
    if (sscanf(line + 1, "%u", &n) == 1 && n)
      capture_begin(n);
    return;
  default:
    return;
  }
  (void)xQueueSend(commands, &command, pdMS_TO_TICKS(100));
}

/* The receive FIFO is polled: the USB Serial/JTAG driver component links a
 * connection monitor into every build that includes it, with or without
 * the profiler, and that monitor does not start under QEMU. */
static void console(void *argument) {
  (void)argument;
  static char line[96];
  size_t length = 0;
  for (;;) {
    /* Lines keep arriving during a capture: the script drives the input
     * the capture measures. */
    if (capture_end && (int32_t)(xTaskGetTickCount() - capture_end) >= 0)
      capture_finish();
    uint8_t bytes[32];
    const uint32_t count = usb_serial_jtag_ll_read_rxfifo(bytes, sizeof(bytes));
    if (count == 0) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    for (uint32_t i = 0; i < count; i++) {
      const uint8_t byte = bytes[i];
      if (byte == '\n' || byte == '\r') {
        line[length] = 0;
        if (length)
          run_line(line);
        length = 0;
      } else if (length < sizeof(line) - 1) {
        line[length++] = (char)byte;
      }
    }
  }
}

void m5_pocket_profile_tools_start(void) {
  commands = xQueueCreate(8, sizeof(input_command));
  if (!commands)
    return;
  if (xTaskCreatePinnedToCore(console, "m5_prof_in", 4096, NULL, 2, NULL, 0) !=
      pdPASS)
    ESP_LOGE(TAG, "no console task");
}

#endif
