/*
 * Device data bridge between a board program and its application.
 *
 * Board -> app: the program publishes named values from any task. A name is
 * the name of a top-level `let` field of the application's device.ts state
 * module. The app host delivers every value changed since the previous frame
 * once, before the frame runs: in JS mode through the `globalThis.m5` realm
 * namespace that main.tsx polls, in AOT mode through the Rust host, which
 * writes the generated `device_<name>_mut()` accessors. Events are one-shot
 * values kept in a FIFO until the next frame consumes them.
 *
 * App -> board: device.ts queues request strings ("<name> <arg> <arg>..."),
 * which the app host drains after every frame and passes, parsed, to the
 * handler the program registered. Requests are delivered on the app task, in
 * the order the application made them.
 *
 * Publishing functions are thread-safe and may block briefly on a mutex; do
 * not call them from an ISR. They return ESP_ERR_INVALID_ARG for a NULL or
 * over-long name, ESP_ERR_NO_MEM when the slot table, the event FIFO or a
 * value allocation is full.
 */
#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Longest value name, in bytes, without the terminator. */
#define M5_POCKET_BRIDGE_NAME_MAX 31
/* Most numeric arguments a parsed request carries. */
#define M5_POCKET_REQUEST_ARGS_MAX 8

typedef enum {
  M5_POCKET_VALUE_NUMBER = 1,
  M5_POCKET_VALUE_TEXT = 2,
  M5_POCKET_VALUE_ARRAY = 3,
} m5_pocket_value_kind;

/* Board -> app values. A bool is published as the number 0 or 1. */
esp_err_t m5_pocket_bridge_set_number(const char *name, double value);
esp_err_t m5_pocket_bridge_set_bool(const char *name, bool value);
/* UTF-8 text; at most CONFIG_M5_POCKET_BRIDGE_TEXT_MAX bytes are kept. */
esp_err_t m5_pocket_bridge_set_text(const char *name, const char *text);
/* At most CONFIG_M5_POCKET_BRIDGE_ARRAY_MAX elements are kept. */
esp_err_t m5_pocket_bridge_set_array(const char *name, const float *values,
                                     size_t count);
/* One-shot board -> app event, delivered exactly once in FIFO order. */
esp_err_t m5_pocket_bridge_post_event(const char *name, double value);

/* A value or event as the app host receives it. Pointers stay valid until
 * the next m5_pocket_bridge_next_value or m5_pocket_bridge_next_event call.
 * struct_size is set by the caller. */
typedef struct {
  size_t struct_size;
  const char *name;
  size_t name_length;
  uint32_t kind; /* m5_pocket_value_kind; events are numbers */
  double number;
  const char *text;
  size_t text_length;
  const float *array;
  size_t array_length;
} m5_pocket_value;

/* Consumer side, called by the app host on the app task once per frame.
 * Return true and fill `out` with the next value changed since it was last
 * taken (or the next queued event); false when none is left. */
bool m5_pocket_bridge_next_value(m5_pocket_value *out);
bool m5_pocket_bridge_next_event(m5_pocket_value *out);

/* An app -> board request. `text` is the whole request string, `name` its
 * first word; `args` holds the following words that parse as numbers, in
 * order, up to M5_POCKET_REQUEST_ARGS_MAX. Pointers are valid during the
 * handler call only. */
typedef struct {
  size_t struct_size;
  const char *text;
  size_t text_length;
  const char *name;
  size_t name_length;
  size_t argc;
  double args[M5_POCKET_REQUEST_ARGS_MAX];
} m5_pocket_request;

typedef void (*m5_pocket_request_fn)(void *context,
                                     const m5_pocket_request *request);

/* Install the request handler (NULL removes it). m5_pocket_app_start
 * installs the one of its configuration. */
void m5_pocket_bridge_set_request_handler(m5_pocket_request_fn handler,
                                          void *context);
/* Parse one request string and call the handler. Called by the app host;
 * a program may call it to inject a request. Returns ESP_ERR_INVALID_ARG for
 * empty text and ESP_ERR_INVALID_STATE without a handler. */
esp_err_t m5_pocket_bridge_dispatch_request(const char *text, size_t length);

/* Requests dropped because no handler was installed, and board values or
 * events refused because the table or FIFO was full. */
typedef struct {
  size_t struct_size;
  uint32_t values_refused;
  uint32_t events_refused;
  uint32_t requests_unhandled;
} m5_pocket_bridge_stats;
esp_err_t m5_pocket_bridge_get_stats(m5_pocket_bridge_stats *out);

#ifdef __cplusplus
}
#endif
