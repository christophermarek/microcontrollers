#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "priv.h"
#include "wb_config.h"

static const char *TAG = "wb";

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)  /* log disconnect */
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "wifi: STA disconnected");
    }
}

static void ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)  /* log IP/mask/gw */
{
    (void)arg;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "wifi: got IP " IPSTR " mask " IPSTR " gw " IPSTR,
                 IP2STR(&event->ip_info.ip),
                 IP2STR(&event->ip_info.netmask),
                 IP2STR(&event->ip_info.gw));
    }
}

void wifi_init_blocking(void)
{
    ESP_LOGI(TAG, "wifi: init STA, SSID=%s", WB_WIFI_SSID);
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event, NULL, &instance_any));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &ip_event, NULL, &instance_got_ip));
    wifi_config_t wcfg = {0};
    strncpy((char *)wcfg.sta.ssid, WB_WIFI_SSID, sizeof(wcfg.sta.ssid) - 1);
    strncpy((char *)wcfg.sta.password, WB_WIFI_PASSWORD, sizeof(wcfg.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi: started, waiting for IP (timeout 10 s)");
    for (int i = 0; i < 100; i++) {  /* 100 * 100 ms = 10 s */
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_netif_ip_info_t info;
        if (esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("STA_DEF"), &info) == ESP_OK) {
            if (info.ip.addr != 0) {
                ESP_LOGI(TAG, "wifi: connected with IP");
                return;
            }
        }
    }
    ESP_LOGW(TAG, "wifi: timeout waiting for IP (10 s)");
}
