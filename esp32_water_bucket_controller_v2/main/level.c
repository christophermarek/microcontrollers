/*
 * level.c - Poll GPIO 13/14/25 every 200 ms. s_level[i]: 0=water 1=dry.
 * pumps_disabled when all three dry; then set_pump(WB_PUMP_OFF). Publishes MQTT only on change.
 */

#include "esp_log.h"
#include "priv.h"

static const char *TAG = "wb";

int s_level[WB_NUM_LEVELS] = {0, 0, 0};
int s_last_level[WB_NUM_LEVELS] = {-1, -1, -1};
bool s_pumps_disabled = true;
bool s_last_pumps_disabled = true;

void read_levels(void)
{
    bool any_change = false;
    for (size_t i = 0; i < WB_NUM_LEVELS; i++) {
        int v = level_gpio_get((int)i);
        if (v != s_last_level[i]) {
            ESP_LOGI(TAG, "state: level[%u] %d -> %d (%s)", (unsigned)i, s_last_level[i], v,
                     v ? "dry" : "water");
            s_last_level[i] = v;
            s_level[i] = v;
            any_change = true;
        } else {
            s_level[i] = v;
        }
    }
    bool prev_disabled = s_pumps_disabled;
    s_pumps_disabled = (s_level[0] != 0 && s_level[1] != 0 && s_level[2] != 0);
    if (s_pumps_disabled != s_last_pumps_disabled) {
        s_last_pumps_disabled = s_pumps_disabled;
        any_change = true;
        ESP_LOGI(TAG, "state: pumps_disabled %d -> %d (L1 L2 L3=%d,%d,%d)",
                 prev_disabled ? 1 : 0, s_pumps_disabled ? 1 : 0,
                 s_level[0], s_level[1], s_level[2]);
    }
    if (s_pumps_disabled && !prev_disabled) {
        ESP_LOGI(TAG, "levels: all-dry -> pump off");
        set_pump(WB_PUMP_OFF);
    }
    if (any_change) {
        publish_levels();
    }
}

void level_timer_cb(void *arg)
{
    (void)arg;
    read_levels();
}
