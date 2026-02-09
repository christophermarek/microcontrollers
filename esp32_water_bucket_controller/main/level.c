/*
 * level.c - Level sensing and pumps_disabled logic for water bucket controller.
 *
 * read_levels() reads level pins via gpio, updates s_level and s_pumps_disabled
 * (true when all three dry). On transition to all-dry calls set_pump(WB_PUMP_OFF).
 * Publishes level on change; pump state ("off") is published from pump.c. level_timer_cb is the 200 ms timer.
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
        ESP_LOGI(TAG, "levels: level %d = %d", i, v);
        if (v != s_last_level[i]) {
            s_last_level[i] = v;
            s_level[i] = v;
            any_change = true;
        } else {
            s_level[i] = v;  /* keep current even if no transition */
        }
    }
    bool prev_disabled = s_pumps_disabled;
    s_pumps_disabled = (s_level[0] != 0 && s_level[1] != 0 && s_level[2] != 0);  /* all high = dry */
    if (s_pumps_disabled != s_last_pumps_disabled) {
        s_last_pumps_disabled = s_pumps_disabled;
        any_change = true;
        ESP_LOGI(TAG, "levels: pumps_disabled=%d (L1=%d L2=%d L3=%d)",
                 s_pumps_disabled ? 1 : 0, s_level[0], s_level[1], s_level[2]);
    }
    if (s_pumps_disabled && !prev_disabled) {
        ESP_LOGI(TAG, "levels: transition to all-dry, turning off pump");
        set_pump(WB_PUMP_OFF);  /* safety: force all off when all dry */
    }
    if (any_change) {
        publish_levels();
    }
}

void level_timer_cb(void *arg)  /* 200 ms periodic; runs in timer task */
{
    (void)arg;
    read_levels();
}
