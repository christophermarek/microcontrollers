/*
 * pump.c - Pump selection and active-pump publishing for water bucket controller.
 *
 * set_pump(index): 0..3 = one pump on (GPIO 16..19), WB_PUMP_OFF = all off.
 * Rejects turn-on when s_pumps_disabled. Uses s_pump_mux; calls pump_gpio_set.
 */

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "priv.h"

static const char *TAG = "wb";

SemaphoreHandle_t s_pump_mux = NULL;
uint8_t s_current_pump = WB_PUMP_OFF;
static uint8_t s_last_published_pump = WB_PUMP_OFF;

void set_pump(uint8_t index)
{
    if (xSemaphoreTake(s_pump_mux, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "set_pump: mutex timeout (100 ms), skipping index=%u", (unsigned)index);
        return;
    }
    if (s_pumps_disabled && index != WB_PUMP_OFF) {  /* allow only "off" when all dry */
        xSemaphoreGive(s_pump_mux);
        ESP_LOGW(TAG, "set_pump: rejected index=%u (pumps_disabled=1, all levels dry)", (unsigned)index);
        return;
    }
    for (size_t i = 0; i < WB_NUM_PUMPS; i++) {
        pump_gpio_set((int)i, 0);  /* all off first; then one on */
    }
    if (index < WB_NUM_PUMPS) {
        pump_gpio_set((int)index, 1);
        ESP_LOGI(TAG, "set_pump: pump %u on", (unsigned)index);
    } else {
        ESP_LOGI(TAG, "set_pump: all pumps off");
    }
    s_current_pump = index;
    xSemaphoreGive(s_pump_mux);
    publish_pump();
}

void publish_pump(void)  /* publish "0".."3" or "off" to water_bucket/state/pump */
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
        pump_char[0] = '0' + s_current_pump;
        payload = pump_char;
    }
    esp_mqtt_client_publish(s_mqtt_client, "water_bucket/state/pump",
                            payload, (int)strlen(payload), 0, 0);
    s_last_published_pump = s_current_pump;
    ESP_LOGD(TAG, "publish_pump: payload=%s", payload);
}
