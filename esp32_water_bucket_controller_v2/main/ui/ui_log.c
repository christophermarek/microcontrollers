#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ui.h"

#define UI_LOG_CAP 96
#define UI_LOG_LEN 64

static char s_log[UI_LOG_CAP][UI_LOG_LEN];
static size_t s_head;
static size_t s_count;
static SemaphoreHandle_t s_mux;

static void format_ts(char *out, size_t out_len)
{
    time_t now = time(NULL);
    if (now > 1700000000) {
        struct tm ti;
        localtime_r(&now, &ti);
        snprintf(out, out_len, "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
        return;
    }
    uint64_t us = (uint64_t)esp_timer_get_time();
    uint32_t s = (uint32_t)(us / 1000000ULL);
    uint32_t h = s / 3600U;
    uint32_t m = (s % 3600U) / 60U;
    uint32_t sec = s % 60U;
    snprintf(out, out_len, "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)sec);
}

void ui_log_init(void)
{
    if (s_mux == NULL) {
        s_mux = xSemaphoreCreateMutex();
    }
}

void ui_log_event(const char *message)
{
    if (s_mux == NULL) {
        ui_log_init();
    }
    if (message == NULL) {
        return;
    }
    if (xSemaphoreTake(s_mux, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    char ts[16];
    format_ts(ts, sizeof(ts));
    snprintf(s_log[s_head], UI_LOG_LEN, "%s %s", ts, message);
    s_head = (s_head + 1U) % UI_LOG_CAP;
    if (s_count < UI_LOG_CAP) {
        s_count++;
    }
    xSemaphoreGive(s_mux);
}

void ui_log_eventf(const char *fmt, ...)
{
    if (fmt == NULL) {
        return;
    }
    char line[48];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    ui_log_event(line);
}

size_t ui_log_count(void)
{
    if (s_mux == NULL) {
        return s_count;
    }
    if (xSemaphoreTake(s_mux, pdMS_TO_TICKS(20)) != pdTRUE) {
        return s_count;
    }
    size_t count = s_count;
    xSemaphoreGive(s_mux);
    return count;
}

void ui_log_get_recent(size_t index_from_newest, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    out[0] = '\0';
    if (s_mux == NULL) {
        return;
    }
    if (xSemaphoreTake(s_mux, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    if (index_from_newest >= s_count) {
        xSemaphoreGive(s_mux);
        return;
    }
    size_t newest = (s_head + UI_LOG_CAP - 1U) % UI_LOG_CAP;
    size_t idx = (newest + UI_LOG_CAP - index_from_newest) % UI_LOG_CAP;
    snprintf(out, out_len, "%s", s_log[idx]);
    xSemaphoreGive(s_mux);
}
