/*
 * ota.c - After OTA boot: PENDING_VERIFY + level GPIO sanity -> mark valid or rollback.
 * ota_start_from_url: one OTA task at a time; downloads via esp_https_ota then reboot.
 */

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "priv.h"

static const char *TAG = "wb";

static atomic_int s_ota_active;

void ota_check_rollback(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        return;
    }
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) != ESP_OK) {
        return;
    }
    if (ota_state != ESP_OTA_IMG_PENDING_VERIFY) {
        return;
    }
    bool diagnostic_ok = true;
    for (int i = 0; i < WB_NUM_LEVELS; i++) {
        int v = level_gpio_get(i);
        if (v != 0 && v != 1) {
            diagnostic_ok = false;
            break;
        }
    }
    if (diagnostic_ok) {
        ESP_LOGI(TAG, "ota: diagnostics ok, mark valid");
        esp_ota_mark_app_valid_cancel_rollback();
    } else {
        ESP_LOGE(TAG, "ota: diagnostics fail, rollback");
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}

static void ota_clear_active(void)
{
    atomic_store_explicit(&s_ota_active, 0, memory_order_release);
}

static void ota_task(void *arg)
{
    char *url = (char *)arg;
    if (url == NULL) {
        ota_clear_active();
        vTaskDelete(NULL);
        return;
    }
    esp_http_client_config_t http_config = {
        .url = url,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };
    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ota: begin %s", esp_err_to_name(err));
        free(url);
        ota_clear_active();
        vTaskDelete(NULL);
        return;
    }
    while (1) {
        err = esp_https_ota_perform(ota_handle);
        if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            continue;
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ota: perform %s", esp_err_to_name(err));
            esp_https_ota_abort(ota_handle);
            free(url);
            ota_clear_active();
            vTaskDelete(NULL);
            return;
        }
        break;
    }
    err = esp_https_ota_finish(ota_handle);
    free(url);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ota: finish %s", esp_err_to_name(err));
        ota_clear_active();
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "ota: reboot");
    esp_restart();
}

void ota_start_from_url(const char *url)
{
    if (url == NULL || strlen(url) < 12) {
        return;
    }
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        ESP_LOGW(TAG, "ota: url must be http(s)://");
        return;
    }
    size_t len = strlen(url) + 1;
    if (len > 256) {
        ESP_LOGW(TAG, "ota: url too long");
        return;
    }
    if (atomic_exchange_explicit(&s_ota_active, 1, memory_order_acq_rel)) {
        ESP_LOGW(TAG, "ota: already in progress");
        return;
    }

    char *copy = malloc(len);
    if (copy == NULL) {
        ESP_LOGE(TAG, "ota: malloc");
        ota_clear_active();
        return;
    }
    memcpy(copy, url, len);
    BaseType_t ok = xTaskCreate(ota_task, "ota", 8192, copy, 5, NULL);
    if (ok != pdPASS) {
        free(copy);
        ota_clear_active();
        ESP_LOGE(TAG, "ota: task create");
    }
}
