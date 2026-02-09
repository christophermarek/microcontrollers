/*
 * Water bucket controller - app_main.
 *
 * Init order (production):
 *   1. NVS flash (required for WiFi and other persistent config).
 *   2. Create pump/level mutex (protects shared state across timer and MQTT task).
 *   3. GPIO init (pump outputs LOW, level inputs; no pull on level pins).
 *   4. WiFi STA connect (block until IP or 10 s timeout).
 *   5. MQTT client init from wb_config.h, register event handler, start client.
 *   6. Create 200 ms periodic timer for level polling.
 *   7. Block forever; all work is done in timer callback and MQTT event handler.
 *
 * On failure: mutex create or MQTT init fail -> log error and block (no recovery).
 * WiFi timeout -> we continue; MQTT may still connect if broker is reachable later.
 */

#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "priv.h"
#include "wb_config.h"

static const char *TAG = "wb";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "app_main: water bucket controller start");
    ESP_LOGI(TAG, "app_main: nvs_flash_init");
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "app_main: create pump mutex");
    s_pump_mux = xSemaphoreCreateMutex();  /* binary mutex for pump/level state */
    if (s_pump_mux == nullptr) {
        ESP_LOGE(TAG, "app_main: mutex create failed, aborting");
        return;
    }
    ESP_LOGI(TAG, "app_main: gpio_init");
    gpio_init();
    ESP_LOGI(TAG, "app_main: netif and event loop");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, "app_main: wifi_init_blocking");
    wifi_init_blocking();
    log_tcp_init();
    ESP_LOGI(TAG, "app_main: mqtt client init uri=%s", WB_MQTT_BROKER_URI);
    if (strstr(WB_MQTT_BROKER_URI, ":8123") != nullptr) {
        ESP_LOGW(TAG, "app_main: port 8123 is usually HTTP (e.g. Home Assistant); use 1883 for MQTT");
    }
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = WB_MQTT_BROKER_URI;
    if (strlen(WB_MQTT_USER) > 0) {
        mqtt_cfg.credentials.username = WB_MQTT_USER;
        mqtt_cfg.credentials.authentication.password = WB_MQTT_PASSWORD;
        ESP_LOGI(TAG, "app_main: mqtt auth user=%s", WB_MQTT_USER);
    }
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == nullptr) {
        ESP_LOGE(TAG, "app_main: mqtt client init failed, blocking");
        vTaskDelay(portMAX_DELAY);
        return;
    }
    ESP_LOGI(TAG, "app_main: mqtt register event handler and start");
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_mqtt_client,
                                                    (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                                    mqtt_event, nullptr));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt_client));
    ESP_LOGI(TAG, "app_main: create level timer 200 ms");
    const esp_timer_create_args_t timer_args = {
        .callback = &level_timer_cb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "level",
        .skip_unhandled_events = false
    };
    esp_timer_handle_t level_timer = nullptr;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &level_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(level_timer, 200000));
    ESP_LOGI(TAG, "app_main: init done, entering main loop (block)");
    vTaskDelay(portMAX_DELAY);
}
