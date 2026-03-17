/*
 * pump.c - set_pump(0..5) enables one decoder output; set_pump(WB_PUMP_OFF) disables EN.
 * Mutex with level timer; rejects turn-on when s_pumps_disabled. s_current_pump always 0..5 or WB_PUMP_OFF for MQTT state.
 */

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "priv.h"

static const char *TAG = "wb";

SemaphoreHandle_t s_pump_mux = NULL;
uint8_t s_current_pump = WB_PUMP_OFF;
bool s_ui_pump_enabled = true;

void set_ui_pump_enabled(bool enabled)
{
    s_ui_pump_enabled = enabled;
    if (!enabled) {
        set_pump(WB_PUMP_OFF);
    }
}

void set_pump(uint8_t index)
{
    if (xSemaphoreTake(s_pump_mux, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "set_pump: mutex timeout (100 ms), skipping index=%u", (unsigned)index);
        return;
    }
    if (!s_ui_pump_enabled && index < WB_NUM_PUMPS) {
        xSemaphoreGive(s_pump_mux);
        ESP_LOGW(TAG, "set_pump: rejected index=%u (ui disabled)", (unsigned)index);
        return;
    }
    if (s_pumps_disabled && index < WB_NUM_PUMPS) {
        xSemaphoreGive(s_pump_mux);
        ESP_LOGW(TAG, "set_pump: rejected index=%u (pumps_disabled)", (unsigned)index);
        return;
    }
    if (index < WB_NUM_PUMPS) {
        pump_decoder_apply(index);
        if (s_current_pump != index) {
            ESP_LOGI(TAG, "state: pump %u -> %u", (unsigned)s_current_pump, (unsigned)index);
        }
        s_current_pump = index;
    } else {
        pump_decoder_apply(WB_PUMP_OFF);
        if (s_current_pump < WB_NUM_PUMPS) {
            ESP_LOGI(TAG, "state: pump %u -> off", (unsigned)s_current_pump);
        }
        s_current_pump = WB_PUMP_OFF;
    }
    xSemaphoreGive(s_pump_mux);
    publish_pump();
}

void publish_pump(void)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGD(TAG, "publish_pump: client null, skip");
        return;
    }
    const char *payload;
    static char pump_char[2] = {'0', '\0'};
    if (s_current_pump >= WB_NUM_PUMPS) {
        payload = "off";
    } else {
        pump_char[0] = (char)('0' + s_current_pump);
        payload = pump_char;
    }
    esp_mqtt_client_publish(s_mqtt_client, "water_bucket/state/pump",
                            payload, (int)strlen(payload), 0, 0);
    ESP_LOGD(TAG, "publish_pump: payload=%s", payload);
}
