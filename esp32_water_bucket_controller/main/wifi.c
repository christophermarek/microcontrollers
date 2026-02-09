#include <stdlib.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "priv.h"
#include "wb_config.h"

#define WIFI_WAIT_IP_MS       6000
#define WIFI_ATTEMPT_MAX     12
#define WIFI_RETRY_DELAY_MS  500

static const char *TAG = "wb";
static SemaphoreHandle_t s_got_ip;

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base != WIFI_EVENT) return;
    if (id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "wifi: STA connected to AP (waiting for DHCP)");
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *ev = (wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG, "wifi: STA disconnected reason=%u rssi=%d", (unsigned)ev->reason, (int)ev->rssi);
        if (s_got_ip != NULL) {
            esp_wifi_connect();
        }
    }
}

static void ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "wifi: got IP " IPSTR " mask " IPSTR " gw " IPSTR,
                 IP2STR(&event->ip_info.ip),
                 IP2STR(&event->ip_info.netmask),
                 IP2STR(&event->ip_info.gw));
        if (s_got_ip != NULL) {
            xSemaphoreGive(s_got_ip);
        }
    }
}

void wifi_init_blocking(void)
{
    esp_log_level_set("wifi", ESP_LOG_DEBUG);
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
    s_got_ip = xSemaphoreCreateBinary();
    if (s_got_ip == NULL) {
        ESP_LOGE(TAG, "wifi: semaphore create failed");
        return;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_scan_config_t scan_cfg = { .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true };
    esp_err_t scan_ok = esp_wifi_scan_start(&scan_cfg, true);
    if (scan_ok == ESP_OK) {
        uint16_t n = 0;
        esp_wifi_scan_get_ap_num(&n);
        ESP_LOGI(TAG, "wifi: scan found %u APs", (unsigned)n);
        if (n > 0 && n <= 64) {
            wifi_ap_record_t *aps = (wifi_ap_record_t *)malloc(n * sizeof(wifi_ap_record_t));
            if (aps && esp_wifi_scan_get_ap_records(&n, aps) == ESP_OK) {
                for (uint16_t i = 0; i < n; i++) {
                    size_t ssid_len = strlen(WB_WIFI_SSID);
                    if (ssid_len <= 32 && memcmp(aps[i].ssid, WB_WIFI_SSID, ssid_len) == 0) {
                        ESP_LOGI(TAG, "wifi: found SSID=%s auth=%u rssi=%d", (char *)aps[i].ssid, (unsigned)aps[i].authmode, (int)aps[i].rssi);
                        break;
                    }
                }
                free(aps);
            }
        }
    } else {
        ESP_LOGW(TAG, "wifi: scan failed %d", (int)scan_ok);
    }
    wifi_config_t wcfg = {0};
    strncpy((char *)wcfg.sta.ssid, WB_WIFI_SSID, sizeof(wcfg.sta.ssid) - 1);
    strncpy((char *)wcfg.sta.password, WB_WIFI_PASSWORD, sizeof(wcfg.sta.password) - 1);
    wcfg.sta.threshold.authmode = WIFI_AUTH_WPA2_WPA3_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI(TAG, "wifi: waiting for IP (%d s per try, up to %d tries)", WIFI_WAIT_IP_MS / 1000, WIFI_ATTEMPT_MAX);
    for (int attempt = 0; attempt < WIFI_ATTEMPT_MAX; attempt++) {
        if (xSemaphoreTake(s_got_ip, pdMS_TO_TICKS(WIFI_WAIT_IP_MS)) == pdTRUE) {
            ESP_LOGI(TAG, "wifi: connected with IP");
            vSemaphoreDelete(s_got_ip);
            s_got_ip = NULL;
            return;
        }
        if (attempt < WIFI_ATTEMPT_MAX - 1) {
            esp_wifi_disconnect();
            vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_DELAY_MS));
            esp_wifi_connect();
        }
    }
    ESP_LOGW(TAG, "wifi: no IP after %d tries (%d s each)", WIFI_ATTEMPT_MAX, WIFI_WAIT_IP_MS / 1000);
    vSemaphoreDelete(s_got_ip);
    s_got_ip = NULL;
}
