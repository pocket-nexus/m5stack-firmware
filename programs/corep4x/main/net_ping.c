/* PING window texts (net_ping.h). */
#include "net_ping.h"
#include <stdio.h>
#include <string.h>

/* Tones of device.ts. */
#define TONE_SUCCESS 1
#define TONE_WARNING 2

const char *const net_ping_names[NET_PING_TARGETS] = {
    "Google DNS",
    "Cloudflare DNS",
    "M5Stack Site",
    "Gateway",
};

const char *const net_ping_hosts[NET_PING_TARGETS] = {
    "8.8.8.8",
    "1.1.1.1",
    "m5stack.com",
    "192.168.1.1",
};

void net_ping_row_set(net_ping_row_t *row, uint8_t dot, uint8_t tone,
                      const char *text) {
  row->dot = dot;
  row->tone = tone;
  snprintf(row->text, sizeof(row->text), "%s", text);
}

size_t net_ping_rows_format(const net_ping_row_t *rows, size_t count, char *out,
                            size_t size) {
  if (size == 0) {
    return 0;
  }
  size_t length = 0;
  out[0] = '\0';
  for (size_t i = 0; i < count && length + 1 < size; i++) {
    const int n = snprintf(out + length, size - length, "%s%c%c%s",
                           i > 0 ? "\n" : "", '0' + (rows[i].dot % 10),
                           '0' + (rows[i].tone % 10), rows[i].text);
    if (n < 0) {
      break;
    }
    length += (size_t)n;
    if (length >= size) {
      length = size - 1;
    }
  }
  return length;
}

uint32_t net_ping_attempt(uint32_t seqno, uint32_t count) {
  if (seqno == 0) {
    return 1;
  }
  return seqno > count ? count : seqno;
}

void net_ping_progress_text(bool reply, uint32_t seqno, uint32_t count,
                            char *out, size_t size) {
  snprintf(out, size, reply ? "Testing (%lu/%lu)..." : "Timeout (%lu/%lu)",
           (unsigned long)net_ping_attempt(seqno, count), (unsigned long)count);
}

void net_ping_result_text(bool ok, uint32_t mean_ms, const char *error,
                          char *out, size_t size) {
  if (ok) {
    snprintf(out, size, "%lu ms", (unsigned long)mean_ms);
  } else {
    snprintf(out, size, "%s", (error && error[0]) ? error : "Failed");
  }
}

uint8_t net_ping_summary_text(uint32_t ok, uint32_t total, char *out,
                              size_t size) {
  if (ok == total) {
    snprintf(out, size, "Test completed: All %lu targets succeeded!",
             (unsigned long)ok);
    return TONE_SUCCESS;
  }
  snprintf(out, size, "Test completed: %lu/%lu targets succeeded.",
           (unsigned long)ok, (unsigned long)total);
  return TONE_WARNING;
}
