/* The TIMER window's SNTP probe, implemented in svc_net.c and driven by
 * svc_rtc.c (ui_function_timer_window.cpp: start_ntp_probe_task,
 * local_time_from_system_clock). */
#pragma once

#include <stdbool.h>
#include <time.h>

/* "timerPage 1": ntpReady false, then the probe runs once (unless a
 * previous probe still runs; that one then reports to this visit): wait up
 * to 4 s for an Ethernet address, sync with ntp.aliyun.com (6 s at most)
 * and publish ntpReady true when the system clock then holds a time of
 * 2020..2098. "timerPage 0": ntpReady false. */
void net_svc_timer_page(bool open);
/* The NTP button found no synchronized system clock: ntpReady false until
 * the next probe. */
void net_svc_sntp_forget(void);
/* local_time_from_system_clock: false before 2020-01-01 00:00:00 UTC;
 * otherwise the system clock as UTC+8 ("CST-8") local time, true when its
 * year is 2020..2098. */
bool net_sntp_local_time(struct tm *out);
