#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ui_tz.h"

#define NVS_NS "wb_ui"
#define KEY_TZ "tz"

static const char *const k_names[] = {
    "UTC",
    "Eastern",
    "Central",
    "Mountain",
    "Pacific",
    "Alaska",
    "Hawaii",
};

static const char *const k_spec[] = {
    "UTC0",
    "EST5EDT,M3.2.0,M11.1.0/2",
    "CST6CDT,M3.2.0,M11.1.0/2",
    "MST7MDT,M3.2.0,M11.1.0/2",
    "PST8PDT,M3.2.0,M11.1.0/2",
    "AKST9AKDT,M3.2.0,M11.1.0/2",
    "HST10",
};

static uint8_t s_tz = 1;

void ui_tz_apply_env(uint8_t idx)
{
    if (idx >= UI_TZ_COUNT) {
        idx = 0;
    }
    setenv("TZ", k_spec[idx], 1);
    tzset();
}

void ui_tz_init(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ui_tz_apply_env(1);
        return;
    }
    uint8_t v = 1;
    esp_err_t e = nvs_get_u8(h, KEY_TZ, &v);
    if (e != ESP_OK || v >= UI_TZ_COUNT) {
        v = 1;
    }
    nvs_close(h);
    s_tz = v;
    ui_tz_apply_env(s_tz);
}

void ui_tz_set(uint8_t idx)
{
    if (idx >= UI_TZ_COUNT) {
        idx = 0;
    }
    s_tz = idx;
    ui_tz_apply_env(s_tz);
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    (void)nvs_set_u8(h, KEY_TZ, s_tz);
    (void)nvs_commit(h);
    nvs_close(h);
}

uint8_t ui_tz_get(void)
{
    return s_tz;
}

const char *ui_tz_name(uint8_t idx)
{
    return k_names[idx % UI_TZ_COUNT];
}

void ui_format_local_clock(char *out, size_t out_sz)
{
    if (out_sz < 12) {
        if (out_sz) {
            out[0] = '\0';
        }
        return;
    }
    time_t t = time(NULL);
    if (t < 1700000000) {
        snprintf(out, out_sz, "No NTP sync");
        return;
    }
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(out, out_sz, "%a %I:%M %p", &tm);
}
