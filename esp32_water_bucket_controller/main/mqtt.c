/*
 * mqtt.c - MQTT client and state publishing for the water bucket controller.
 *
 * Topics:
 *   water_bucket/cmd/pump   (subscribe) Payload: "0".."3" = pump index, "off" = all off.
 *   water_bucket/state/level_1..3  (publish) Payload: "0" = water at level, "1" = dry.
 *   water_bucket/state/pump (publish) Payload: "0".."3" or "off" (off = all off).
 *
 * On MQTT_EVENT_CONNECTED we subscribe to cmd and re-publish full state.
 * On MQTT_EVENT_DATA for cmd topic we parse payload and call set_pump().
 */

#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "priv.h"

static const char *TAG = "wb";

esp_mqtt_client_handle_t s_mqtt_client = NULL;

static const char *s_topic_cmd = "water_bucket/cmd/pump";
static const char *s_topic_state_level1 = "water_bucket/state/level_1";
static const char *s_topic_state_level2 = "water_bucket/state/level_2";
static const char *s_topic_state_level3 = "water_bucket/state/level_3";

void publish_levels(void)  /* level_1..3 as "0" (wet) or "1" (dry) */
{
    if (s_mqtt_client == NULL) {
        ESP_LOGD(TAG, "publish_levels: client null, skip");
        return;
    }
    char buf[2] = {'0', '\0'};  /* reusable single-char payload */
    buf[0] = s_level[0] ? '1' : '0';
    esp_mqtt_client_publish(s_mqtt_client, s_topic_state_level1, buf, 1, 0, 0);
    buf[0] = s_level[1] ? '1' : '0';
    esp_mqtt_client_publish(s_mqtt_client, s_topic_state_level2, buf, 1, 0, 0);
    buf[0] = s_level[2] ? '1' : '0';
    esp_mqtt_client_publish(s_mqtt_client, s_topic_state_level3, buf, 1, 0, 0);
    ESP_LOGD(TAG, "publish_levels: L1=%d L2=%d L3=%d", s_level[0], s_level[1], s_level[2]);
}

void publish_full_state(void)  /* levels + pump; used after connect to re-sync HA */
{
    ESP_LOGI(TAG, "mqtt: publishing full state (levels, pump)");
    publish_levels();
    publish_pump();
}

void mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *data)  /* subscribe on connect; parse cmd on DATA */
{
    (void)arg;
    (void)base;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "mqtt: connected, subscribing to %s", s_topic_cmd);
        esp_mqtt_client_subscribe(event->client, s_topic_cmd, 0);
        publish_full_state();  /* re-sync HA with current levels/pump */
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "mqtt: disconnected");
        break;
    case MQTT_EVENT_DATA: {
        size_t topic_len = event->topic_len;
        if (topic_len > 64) {
            topic_len = 64;
        }
        char topic_buf[65];
        memcpy(topic_buf, event->topic, topic_len);
        topic_buf[topic_len] = '\0';
        if (strcmp(topic_buf, s_topic_cmd) != 0) {
            ESP_LOGD(TAG, "mqtt: DATA topic=%s (ignored)", topic_buf);
            break;
        }
        uint8_t pump_index = WB_PUMP_OFF;  /* parse "0".."3" or "off" */
        if (event->data_len == 1) {
            char c = event->data[0];
            if (c >= '0' && c <= '3') {
                pump_index = (uint8_t)(c - '0');
            }
        } else if (event->data_len == 3 &&
                   event->data[0] == 'o' && event->data[1] == 'f' && event->data[2] == 'f') {
            pump_index = WB_PUMP_OFF;
        }
        ESP_LOGI(TAG, "mqtt: cmd received payload_len=%d -> pump_index=%u", (int)event->data_len, (unsigned)pump_index);
        set_pump(pump_index);
        break;
    }
    default:
        break;
    }
}
