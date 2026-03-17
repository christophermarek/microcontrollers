#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "ui_pages_internal.h"
#include "ui_tz.h"

#define STZ_LINES 13
#define STZ_VIS 7
#define STZ_SEG_MAX 96

typedef struct {
    uint8_t line;
    uint8_t first;
    char row[17];
} StzSeg;

static size_t stz_build_segments(const ui_state_t *st, StzSeg *out, size_t maxn)
{
    char full[STZ_LINES][96];
    snprintf(full[0], sizeof(full[0]), "Contrast: %u",
             (unsigned)g_ui_contrast_levels[st->settings_contrast_idx]);
    snprintf(full[1], sizeof(full[1]), "Timezone: %s", ui_tz_name(ui_tz_get()));
    {
        char clk[48];
        ui_format_local_clock(clk, sizeof(clk));
        snprintf(full[2], sizeof(full[2]), "Time: %s", clk);
    }
    snprintf(full[3], sizeof(full[3]), "5 Gal Controller");
    snprintf(full[4], sizeof(full[4]), "MQTT: %s",
             ui_runtime_mqtt_connected() ? "CONNECTED" : "DISCONNECTED");
    snprintf(full[5], sizeof(full[5]), "WIFI: %s",
             ui_runtime_wifi_connected() ? "CONNECTED" : "DISCONNECTED");
    {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            snprintf(full[6], sizeof(full[6]), "RSSI: %d dBm", ap.rssi);
        } else {
            snprintf(full[6], sizeof(full[6]), "RSSI: not connected");
        }
    }
    {
        esp_netif_t *n = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_ip_info_t ip;
        if (n && esp_netif_get_ip_info(n, &ip) == ESP_OK) {
            snprintf(full[7], sizeof(full[7]), "IP: %u.%u.%u.%u", IP2STR(&ip.ip));
        } else {
            snprintf(full[7], sizeof(full[7]), "IP: not assigned");
        }
    }
    snprintf(full[8], sizeof(full[8]), "Uptime: %lu seconds", (unsigned long)ui_runtime_uptime_s());
    snprintf(full[9], sizeof(full[9]), "Heap free: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
    {
        const esp_app_desc_t *desc = esp_app_get_description();
        snprintf(full[10], sizeof(full[10]), "Firmware: %s", desc->version);
    }
    {
        time_t tt = time(NULL);
        snprintf(full[11], sizeof(full[11]), "NTP: %s",
                 tt > 1700000000 ? "synchronized" : "waiting for sync");
    }
    snprintf(full[12], sizeof(full[12]), "Safety: %s",
             s_pumps_disabled
                 ? "locked, all buckets read dry"
                 : "ready, pumps allowed");

    size_t nseg = 0;
    for (int L = 1; L <= STZ_LINES; L++) {
        const char *p = full[L - 1];
        size_t len = strlen(p);
        size_t off = 0;
        int isfirst = 1;
        while (off < len && nseg < maxn) {
            out[nseg].line = (uint8_t)L;
            out[nseg].first = (uint8_t)isfirst;
            if (isfirst) {
                char num[8];
                int nl = snprintf(num, sizeof(num), "%d ", L);
                if (nl < 0) {
                    nl = 0;
                }
                size_t room = 16 - (size_t)nl;
                if (room > len - off) {
                    room = len - off;
                }
                memset(out[nseg].row, ' ', 16);
                memcpy(out[nseg].row, num, (size_t)nl);
                memcpy(out[nseg].row + nl, p + off, room);
                out[nseg].row[16] = '\0';
                off += room;
            } else {
                size_t take = len - off;
                if (take > 13) {
                    take = 13;
                }
                memset(out[nseg].row, ' ', 16);
                out[nseg].row[0] = ' ';
                out[nseg].row[1] = ' ';
                out[nseg].row[2] = ' ';
                memcpy(out[nseg].row + 3, p + off, take);
                out[nseg].row[16] = '\0';
                off += take;
            }
            isfirst = 0;
            nseg++;
        }
    }
    return nseg;
}

static void stz_apply_scroll(ui_state_t *s, const StzSeg *segs, size_t n)
{
    uint16_t max_sc = n > STZ_VIS ? (uint16_t)(n - STZ_VIS) : 0;
    if (s->cursor == 0) {
        s->scroll = 0;
        return;
    }
    int fs = -1;
    for (size_t i = 0; i < n; i++) {
        if (segs[i].line == s->cursor) {
            fs = (int)i;
            break;
        }
    }
    if (fs < 0) {
        s->scroll = 0;
        return;
    }
    uint16_t sc = (uint16_t)fs;
    if (sc > max_sc) {
        sc = max_sc;
    }
    s->scroll = sc;
}

void ui_settings_clamp_scroll(ui_state_t *s)
{
    StzSeg segs[STZ_SEG_MAX];
    size_t n = stz_build_segments(s, segs, STZ_SEG_MAX);
    stz_apply_scroll(s, segs, n);
}

void ui_page_build_settings(const ui_state_t *state, ui_frame_t *frame)
{
    StzSeg segs[STZ_SEG_MAX];
    size_t n = stz_build_segments(state, segs, STZ_SEG_MAX);
    ui_state_t v = *state;
    stz_apply_scroll(&v, segs, n);
    uint16_t sc = v.scroll;
    ui_pages_header_title(frame, "SETTINGS");
    for (int r = 1; r <= 7; r++) {
        size_t idx = (size_t)sc + (size_t)r - 1;
        if (idx < n) {
            ui_pages_set_line(frame->rows[r], segs[idx].row);
        } else {
            ui_pages_set_line(frame->rows[r], "");
        }
    }
    uint8_t c = state->cursor;
    if (c == 0) {
        frame->invert_row = 0;
    } else {
        int inv = -1;
        for (int r = 1; r <= 7; r++) {
            size_t idx = (size_t)sc + (size_t)r - 1;
            if (idx < n && segs[idx].line == c) {
                inv = r;
                break;
            }
        }
        frame->invert_row = inv > 0 ? inv : 1;
    }
}
