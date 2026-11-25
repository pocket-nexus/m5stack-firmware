/* JS mode: the bundle runs in the QuickJS guest. After boot the host installs
 * the realm namespace `globalThis.m5`:
 *
 *   m5.sync(number, text, array, event)
 *       Calls number(name, value), text(name, value) or array(name, values)
 *       once for every board value changed since the previous call, then
 *       event(name, value) for every queued event. A callback that is not a
 *       function skips its kind. Returns the number of calls made.
 *   m5.request(text)
 *       Passes one request string to the program's request handler. Returns
 *       true when a handler received it.
 *   m5.textures(bind)
 *       Calls bind(name, handle) for every image name
 *       m5_pocket_app_set_texture bound or unbound (-1) since the previous
 *       call; js/m5-bridge.ts passes registerTexture. Returns the call count.
 *   m5.mode
 *       "js".
 *
 * main.tsx calls them from an onFrame callback (js/m5-bridge.ts). */
#include "app_internal.h"
#include "esp_log.h"
#include "pocket/guest_idf.h"
#include "pocket/package_source.h"
#include "pocket/textures.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "m5_pocket_app";

typedef struct {
  pocket_runtime *runtime;
  pocket_guest_idf *guest;
  pocket_memory_package package;
  /* Name bindings m5.textures has not delivered yet. */
  struct {
    char *name;
    int32_t handle;
  } bindings[M5_POCKET_APP_TEXTURES_MAX];
} js_backend;

/* The booted backend, for the m5 namespace (one app runs at a time). */
static js_backend *active;

static JSValue call_value(JSContext *ctx, JSValueConst callback,
                          const m5_pocket_value *value) {
  JSValue argv[2];
  argv[0] = JS_NewStringLen(ctx, value->name, value->name_length);
  switch (value->kind) {
  case M5_POCKET_VALUE_TEXT:
    argv[1] = JS_NewStringLen(ctx, value->text, value->text_length);
    break;
  case M5_POCKET_VALUE_ARRAY:
    argv[1] = JS_NewArray(ctx);
    for (size_t i = 0; i < value->array_length && !JS_IsException(argv[1]);
         i++) {
      if (JS_SetPropertyUint32(ctx, argv[1], (uint32_t)i,
                               JS_NewFloat64(ctx, value->array[i])) < 0) {
        JS_FreeValue(ctx, argv[1]);
        argv[1] = JS_EXCEPTION;
      }
    }
    break;
  default:
    argv[1] = JS_NewFloat64(ctx, value->number);
    break;
  }
  JSValue result = JS_EXCEPTION;
  if (!JS_IsException(argv[0]) && !JS_IsException(argv[1]))
    result = JS_Call(ctx, callback, JS_UNDEFINED, 2, argv);
  JS_FreeValue(ctx, argv[0]);
  JS_FreeValue(ctx, argv[1]);
  return result;
}

static JSValue m5_sync(JSContext *ctx, JSValueConst this_value, int argc,
                       JSValueConst *argv) {
  (void)this_value;
  JSValueConst number = argc > 0 ? argv[0] : JS_UNDEFINED;
  JSValueConst text = argc > 1 ? argv[1] : JS_UNDEFINED;
  JSValueConst array = argc > 2 ? argv[2] : JS_UNDEFINED;
  JSValueConst event = argc > 3 ? argv[3] : JS_UNDEFINED;
  int32_t calls = 0;
  m5_pocket_value value = {.struct_size = sizeof(value)};
  while (m5_pocket_bridge_next_value(&value)) {
    JSValueConst callback = value.kind == M5_POCKET_VALUE_TEXT    ? text
                            : value.kind == M5_POCKET_VALUE_ARRAY ? array
                                                                  : number;
    if (!JS_IsFunction(ctx, callback))
      continue;
    JSValue result = call_value(ctx, callback, &value);
    if (JS_IsException(result))
      return result;
    JS_FreeValue(ctx, result);
    calls++;
  }
  while (m5_pocket_bridge_next_event(&value)) {
    if (!JS_IsFunction(ctx, event))
      continue;
    JSValue result = call_value(ctx, event, &value);
    if (JS_IsException(result))
      return result;
    JS_FreeValue(ctx, result);
    calls++;
  }
  return JS_NewInt32(ctx, calls);
}

static JSValue m5_request(JSContext *ctx, JSValueConst this_value, int argc,
                          JSValueConst *argv) {
  (void)this_value;
  if (argc < 1 || !JS_IsString(argv[0]))
    return JS_ThrowTypeError(ctx, "m5.request expects a string");
  size_t length = 0;
  const char *text = JS_ToCStringLen(ctx, &length, argv[0]);
  if (!text)
    return JS_EXCEPTION;
  esp_err_t err = m5_pocket_bridge_dispatch_request(text, length);
  JS_FreeCString(ctx, text);
  return JS_NewBool(ctx, err == ESP_OK);
}

static JSValue m5_textures(JSContext *ctx, JSValueConst this_value, int argc,
                           JSValueConst *argv) {
  (void)this_value;
  if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
    return JS_ThrowTypeError(ctx, "m5.textures expects a function");
  int32_t calls = 0;
  for (int i = 0; active && i < M5_POCKET_APP_TEXTURES_MAX; i++) {
    if (!active->bindings[i].name)
      continue;
    JSValue args[2] = {JS_NewString(ctx, active->bindings[i].name),
                       JS_NewInt32(ctx, active->bindings[i].handle)};
    free(active->bindings[i].name);
    active->bindings[i].name = NULL;
    JSValue result = JS_IsException(args[0])
                         ? JS_EXCEPTION
                         : JS_Call(ctx, argv[0], JS_UNDEFINED, 2, args);
    JS_FreeValue(ctx, args[0]);
    if (JS_IsException(result))
      return result;
    JS_FreeValue(ctx, result);
    calls++;
  }
  return JS_NewInt32(ctx, calls);
}

static esp_err_t install_namespace(JSContext *ctx) {
  JSValue m5 = JS_NewObject(ctx);
  if (JS_IsException(m5))
    return ESP_ERR_NO_MEM;
  bool failed =
      JS_SetPropertyStr(ctx, m5, "sync",
                        JS_NewCFunction(ctx, m5_sync, "sync", 4)) < 0 ||
      JS_SetPropertyStr(ctx, m5, "request",
                        JS_NewCFunction(ctx, m5_request, "request", 1)) < 0 ||
      JS_SetPropertyStr(ctx, m5, "textures",
                        JS_NewCFunction(ctx, m5_textures, "textures", 1)) < 0 ||
      JS_SetPropertyStr(ctx, m5, "mode", JS_NewString(ctx, "js")) < 0;
  if (failed) {
    JS_FreeValue(ctx, m5);
    return ESP_ERR_NO_MEM;
  }
  JSValue global = JS_GetGlobalObject(ctx);
  int set = JS_SetPropertyStr(ctx, global, "m5", m5);
  JS_FreeValue(ctx, global);
  return set < 0 ? ESP_ERR_NO_MEM : ESP_OK;
}

static esp_err_t check(int32_t status, const char *what) {
  if (status == POCKET_STATUS_OK)
    return ESP_OK;
  ESP_LOGE(TAG, "%s: status %d", what, (int)status);
  return ESP_FAIL;
}

esp_err_t m5_pocket_backend_boot(m5_pocket_app *app) {
  const m5_pocket_app_config *config = &app->config;
  const m5_pocket_app_image *image = config->image;
  if (!image->bundle || !image->bundle_size || !image->facts ||
      !image->profile_hash)
    return ESP_ERR_INVALID_ARG;
  js_backend *js = calloc(1, sizeof(*js));
  if (!js)
    return ESP_ERR_NO_MEM;
  app->backend = js;
  pocket_runtime_config runtime_config = {
      .struct_size = sizeof(runtime_config),
      .host_abi = image->host_abi,
      .tick_hz = image->tick_hz,
      .heap_limit = config->heap_limit,
      .frame_buffer_budget = app->devices.display_bytes,
      .profile = image->facts,
  };
  memcpy(runtime_config.profile_hash, image->profile_hash, 32);
  js->runtime = pocket_runtime_create(&runtime_config);
  if (!js->runtime) {
    ESP_LOGE(TAG, "pocket_runtime_create failed");
    return ESP_FAIL;
  }
  const pocket_guest_idf_config guest_config = {
      .heap_limit =
          config->js_heap_limit ? config->js_heap_limit : 2 * 1024 * 1024,
      .stack_limit =
          config->js_stack_limit ? config->js_stack_limit : 64 * 1024,
      .prefer_psram = config->js_prefer_psram,
  };
  js->guest = pocket_guest_idf_create(&guest_config);
  if (!js->guest) {
    ESP_LOGE(TAG, "pocket_guest_idf_create failed");
    return ESP_ERR_NO_MEM;
  }
  esp_err_t err = check(pocket_runtime_set_guest(
                            js->runtime, pocket_guest_idf_vtable(), js->guest),
                        "set_guest");
  js->package = (pocket_memory_package){.data = image->bundle,
                                        .size = image->bundle_size};
  if (err == ESP_OK)
    err = check(pocket_runtime_set_package_source(
                    js->runtime, pocket_memory_package_vtable(), &js->package),
                "set_package_source");
  for (size_t i = 0; err == ESP_OK && i < app->devices.input_count; i++)
    err = check(pocket_runtime_add_input_source(js->runtime,
                                                app->devices.inputs[i],
                                                app->devices.input_contexts[i]),
                "add_input_source");
  pocket_renderer_id renderer = POCKET_RENDERER_SOFTWARE;
  if (err == ESP_OK && app->devices.renderer) {
    renderer = pocket_runtime_add_renderer(js->runtime, app->devices.renderer,
                                           app->devices.renderer_context);
    if (renderer == POCKET_RENDERER_INVALID)
      err = check(POCKET_STATUS_FAILED, "add_renderer");
  }
  if (err == ESP_OK)
    err = check(pocket_runtime_add_display(js->runtime, app->devices.display,
                                           app->devices.display_context,
                                           renderer),
                "add_display");
  if (err != ESP_OK)
    return err;
  int32_t boot = pocket_runtime_boot(js->runtime);
  if (boot != POCKET_STATUS_OK) {
    ESP_LOGE(TAG, "boot status %d: %s", (int)boot,
             pocket_guest_idf_error(js->guest));
    return ESP_FAIL;
  }
  err = install_namespace(pocket_guest_idf_context(js->guest));
  if (err != ESP_OK)
    return err;
  active = js;
  pocket_runtime_timing timing = {.struct_size = sizeof(timing)};
  if (pocket_runtime_get_timing(js->runtime, &timing) != POCKET_STATUS_OK)
    return ESP_FAIL;
  app->cadence_hz = timing.cadence_hz;
  return ESP_OK;
}

int32_t m5_pocket_backend_tick(m5_pocket_app *app) {
  js_backend *js = app->backend;
  int32_t status = pocket_runtime_tick(js->runtime);
  if (status != POCKET_STATUS_OK && app->stats.frame_errors == 0)
    ESP_LOGE(TAG, "guest: %s", pocket_guest_idf_error(js->guest));
  return status;
}

int32_t m5_pocket_backend_skip(m5_pocket_app *app) {
  js_backend *js = app->backend;
  int32_t status = pocket_runtime_advance(js->runtime);
  if (status == POCKET_STATUS_OK)
    status = pocket_runtime_skip_presentation(js->runtime);
  if (status != POCKET_STATUS_OK && app->stats.frame_errors == 0)
    ESP_LOGE(TAG, "guest: %s", pocket_guest_idf_error(js->guest));
  return status;
}

pocket_runtime *m5_pocket_backend_runtime(m5_pocket_app *app) {
  js_backend *js = app->backend;
  return js ? js->runtime : NULL;
}

void m5_pocket_backend_destroy(m5_pocket_app *app) {
  js_backend *js = app->backend;
  if (!js)
    return;
  if (js->runtime)
    pocket_runtime_destroy(js->runtime);
  if (js->guest)
    pocket_guest_idf_destroy(js->guest);
  for (int i = 0; i < M5_POCKET_APP_TEXTURES_MAX; i++)
    free(js->bindings[i].name);
  if (active == js)
    active = NULL;
  free(js);
  app->backend = NULL;
}

int32_t m5_pocket_backend_upload_texture(m5_pocket_app *app,
                                         const uint16_t *pixels, uint32_t width,
                                         uint32_t height) {
  js_backend *js = app->backend;
  return pocket_texture_upload(js->runtime, pixels,
                               (size_t)width * height * sizeof(uint16_t), width,
                               height, 0 /* PSM_5650 */);
}

void m5_pocket_backend_free_texture(m5_pocket_app *app, int32_t handle) {
  js_backend *js = app->backend;
  pocket_texture_free(js->runtime, handle);
}

esp_err_t m5_pocket_backend_bind_texture(m5_pocket_app *app, const char *name,
                                         int32_t handle) {
  js_backend *js = app->backend;
  int slot = -1;
  for (int i = 0; i < M5_POCKET_APP_TEXTURES_MAX; i++) {
    if (js->bindings[i].name && strcmp(js->bindings[i].name, name) == 0) {
      slot = i; /* a newer binding replaces an undelivered one */
      break;
    }
    if (!js->bindings[i].name && slot < 0)
      slot = i;
  }
  if (slot < 0)
    return ESP_ERR_NO_MEM;
  if (!js->bindings[slot].name) {
    js->bindings[slot].name = strdup(name);
    if (!js->bindings[slot].name)
      return ESP_ERR_NO_MEM;
  }
  js->bindings[slot].handle = handle;
  return ESP_OK;
}
