#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "ui_pages_internal.h"
#include "ui_tz.h"

#define STZ_MAX 15
#define STZ_VIS 7
#define STZ_TZ 3

void ui_settings_clamp_scroll(ui_state_t *s)
{
    uint16_t max_sc = (STZ_MAX > STZ_VIS) ? (uint16_t)(STZ_MAX - STZ_VIS) : 0;
    uint8_t c = s->cursor;
    if (c == 0 || c == 1) {
        s->scroll = 0;
    } else {
        if ((uint16_t)c < s->scroll + 1) {
            s->scroll = (uint16_t)(c - 1);
        }
        if ((uint16_t)c > s->scroll + STZ_VIS) {
            s->scroll = (uint16_t)(c - STZ_VIS);
        }
    }
    if (s->scroll > max_sc) {
        s->scroll = max_sc;
    }
}

void ui_page_build_settings(const ui_state_t *state, ui_frame_t *frame)
{
    ui_pages_header_title(frame, "SETTINGS");
    ui_state_t v = *state;
    ui_settings_clamp_scroll(&v);
    uint16_t sc = v.scroll;
    for (int r = 1; r <= 7; r++) {
        int k = (int)sc + r;
        if (k < 1 || k > STZ_MAX) {
            ui_pages_set_line(frame->rows[r], "");
            continue;
        }
        if (k == 1) {
            ui_pages_set_linef(frame->rows[r], "Flip:%s", state->settings_flip ? "ON" : "OFF");
        } else if (k == 2) {
            ui_pages_set_linef(frame->rows[r], "Contrast:%u", (unsigned)g_ui_contrast_levels[state->settings_contrast_idx]);
        } else if (k == STZ_TZ) {
            ui_pages_set_linef(frame->rows[r], "TZ:%s", ui_tz_name(ui_tz_get()));
        } else if (k == 4) {
            char clk[20];
            ui_format_local_clock(clk, sizeof(clk));
            ui_pages_set_linef(frame->rows[r], "%s", clk);
        } else if (k == 5) {
            ui_pages_set_line(frame->rows[r], "5 Gal Controller");
        } else if (k == 6) {
            ui_pages_set_linef(frame->rows[r], "MQTT:%s", ui_runtime_mqtt_connected() ? "ON" : "OFF");
        } else if (k == 7) {
            ui_pages_set_linef(frame->rows[r], "WIFI:%s", ui_runtime_wifi_connected() ? "ON" : "OFF");
        } else if (k == 8) {
            wifi_ap_record_t ap;
            if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
                ui_pages_set_linef(frame->rows[r], "RSSI:%d", ap.rssi);
            } else {
                ui_pages_set_line(frame->rows[r], "RSSI:--");
            }
        } else if (k == 9) {
            esp_netif_t *n = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            esp_netif_ip_info_t ip;
            if (n && esp_netif_get_ip_info(n, &ip) == ESP_OK) {
                ui_pages_set_linef(frame->rows[r], "%u.%u.%u.%u", IP2STR(&ip.ip));
            } else {
                ui_pages_set_line(frame->rows[r], "IP:--");
            }
        } else if (k == 10) {
            ui_pages_set_linef(frame->rows[r], "UP:%lus", (unsigned long)ui_runtime_uptime_s());
        } else if (k == 11) {
            ui_pages_set_linef(frame->rows[r], "HEAP:%u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
        } else if (k == 12) {
            const esp_app_desc_t *desc = esp_app_get_description();
            ui_pages_set_linef(frame->rows[r], "FW:%s", desc->version);
        } else if (k == 13) {
            time_t tt = time(NULL);
            ui_pages_set_line(frame->rows[r], tt > 1700000000 ? "NTP:sync" : "NTP:wait");
        } else if (k == 14) {
            ui_pages_set_linef(frame->rows[r], "PumpUI:%s", s_ui_pump_enabled ? "ON" : "OFF");
        } else if (k == 15) {
            ui_pages_set_linef(frame->rows[r], "Safe:%s", s_pumps_disabled ? "LOCK" : "OK");
        }
    }
    uint8_t c = state->cursor;
    if (c == 0) {
        frame->invert_row = 0;
    } else {
        int inv = (int)c - (int)sc;
        if (inv < 1) {
            inv = 1;
        }
        if (inv > 7) {
            inv = 7;
        }
        frame->invert_row = inv;
    }
}
