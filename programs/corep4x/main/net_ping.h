/* The texts of the PING window (ui_function_ping_window.cpp): the four
 * target rows the board publishes as `pingRows` and the summary lines. No
 * ESP-IDF dependency, so the formatting is testable on a host. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NET_PING_TARGETS 4
/* PING_COUNT, PING_INTERVAL_MS, PING_TIMEOUT_MS and the payload size. */
#define NET_PING_COUNT 4
#define NET_PING_INTERVAL_MS 500
#define NET_PING_TIMEOUT_MS 1000
#define NET_PING_DATA_SIZE 32
/* The wait for a session's end: count x (timeout + interval) + 3 s. */
#define NET_PING_SESSION_MS                                                    \
  (NET_PING_COUNT * (NET_PING_TIMEOUT_MS + NET_PING_INTERVAL_MS) + 3000)
#define NET_PING_TEXT_MAX 64

/* One row: the dot tone, the result text tone (0 muted, 1 success, 2
 * warning, 3 error) and the result text. */
typedef struct {
  uint8_t dot;
  uint8_t tone;
  char text[NET_PING_TEXT_MAX];
} net_ping_row_t;

/* Label names and default hosts of the four targets (the gateway's host is
 * the DHCP gateway, or 192.168.1.1 without one). */
extern const char *const net_ping_names[NET_PING_TARGETS];
extern const char *const net_ping_hosts[NET_PING_TARGETS];

void net_ping_row_set(net_ping_row_t *row, uint8_t dot, uint8_t tone,
                      const char *text);
/* "<dot digit><tone digit><text>" lines joined by "\n"; returns the
 * length written (truncated to size - 1). */
size_t net_ping_rows_format(const net_ping_row_t *rows, size_t count, char *out,
                            size_t size);
/* ping_attempt_display_index: seqno 0 shows as 1, above count as count. */
uint32_t net_ping_attempt(uint32_t seqno, uint32_t count);
/* "Testing (%lu/%lu)..." after a reply, "Timeout (%lu/%lu)" after a
 * timeout. */
void net_ping_progress_text(bool reply, uint32_t seqno, uint32_t count,
                            char *out, size_t size);
/* "%lu ms" (mean round trip) when ok, else the error text or "Failed". */
void net_ping_result_text(bool ok, uint32_t mean_ms, const char *error,
                          char *out, size_t size);
/* "Test completed: All %lu targets succeeded!" (success) or "Test
 * completed: %lu/%lu targets succeeded." (warning); returns the tone. */
uint8_t net_ping_summary_text(uint32_t ok, uint32_t total, char *out,
                              size_t size);
