/*
 * mqtt.c - MQTT client and state publishing for the water bucket controller.
 *
 * Topics:
 *   water_bucket/cmd/pump   (subscribe) Payload: "0".."3" = pump index, "off" = all off.
 *   water_bucket/state/level_1..3  (publish) Payload: "0" = water at level, "1" = dry.
 *   water_bucket/state/pump (publish) Payload: "0".."3" or "off" (off = all off).
 *
 * On MQTT_EVENT_CONNECTED we subscribe to cmd, publish Home Assistant discovery
 * (retained), re-publish full state, then on MQTT_EVENT_DATA for cmd parse and set_pump().
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "priv.h"

static const char *TAG = "wb";

esp_mqtt_client_handle_t s_mqtt_client = NULL;

static const char *s_topic_cmd = "water_bucket/cmd/pump";
static const char *s_topic_state_level1 = "water_bucket/state/level_1";
static const char *s_topic_state_level2 = "water_bucket/state/level_2";
static const char *s_topic_state_level3 = "water_bucket/state/level_3";
static const char *s_topic_status = "water_bucket/status";

#define DISCOVERY_PREFIX "homeassistant"
#define DISCOVERY_BUF_SIZE 640

static void publish_discovery(void)
{
    uint8_t mac[6];
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) != ESP_OK) {
        ESP_LOGW(TAG, "mqtt: discovery skipped (wifi mac unavailable)");
        return;
    }
    char device_id[32];
    snprintf(device_id, sizeof(device_id), "water_bucket_%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    char buf[DISCOVERY_BUF_SIZE];
    int len;
    esp_mqtt_client_handle_t c = s_mqtt_client;
    if (c == NULL) return;

    static const char *level_names[] = { "Level 1", "Level 2", "Level 3" };
    const char *level_topics[] = { s_topic_state_level1, s_topic_state_level2, s_topic_state_level3 };
    static const char *level_uids[] = { "water_bucket_level_1", "water_bucket_level_2", "water_bucket_level_3" };

    for (int i = 0; i < 3; i++) {
        len = snprintf(buf, sizeof(buf),
            "{\"name\":\"%s\",\"state_topic\":\"%s\",\"payload_on\":\"1\",\"payload_off\":\"0\","
            "\"unique_id\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\","
            "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"Water Bucket\",\"model\":\"Water Bucket Controller\",\"manufacturer\":\"DIY\"}}",
            level_names[i], level_topics[i], level_uids[i], s_topic_status, device_id);
        if (len <= 0 || len >= (int)sizeof(buf)) continue;
        char topic[80];
        snprintf(topic, sizeof(topic), "%s/binary_sensor/%s/config", DISCOVERY_PREFIX, level_uids[i]);
        esp_mqtt_client_publish(c, topic, buf, len, 1, 1);
    }

    static const char *pump_names[] = { "Pump 0", "Pump 1", "Pump 2", "Pump 3" };
    static const char *pump_uids[] = { "water_bucket_pump_0", "water_bucket_pump_1", "water_bucket_pump_2", "water_bucket_pump_3" };
    static const char pump_payload_on[] = { '0', '1', '2', '3' };
    static const char *vt_templates[] = {
        "{{ 'ON' if value == '0' else 'OFF' }}",
        "{{ 'ON' if value == '1' else 'OFF' }}",
        "{{ 'ON' if value == '2' else 'OFF' }}",
        "{{ 'ON' if value == '3' else 'OFF' }}"
    };
    for (int i = 0; i < 4; i++) {
        len = snprintf(buf, sizeof(buf),
            "{\"name\":\"%s\",\"command_topic\":\"water_bucket/cmd/pump\",\"state_topic\":\"water_bucket/state/pump\","
            "\"payload_on\":\"%c\",\"payload_off\":\"off\",\"value_template\":\"%s\",\"state_on\":\"ON\",\"state_off\":\"OFF\","
            "\"unique_id\":\"%s\",\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\","
            "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"Water Bucket\",\"model\":\"Water Bucket Controller\",\"manufacturer\":\"DIY\"}}",
            pump_names[i], pump_payload_on[i], vt_templates[i], pump_uids[i], s_topic_status, device_id);
        if (len <= 0 || len >= (int)sizeof(buf)) continue;
        char topic[80];
        snprintf(topic, sizeof(topic), "%s/switch/%s/config", DISCOVERY_PREFIX, pump_uids[i]);
        esp_mqtt_client_publish(c, topic, buf, len, 1, 1);
    }

    esp_mqtt_client_publish(c, "homeassistant/select/water_bucket_pump/config", "", 0, 1, 1);

    ESP_LOGI(TAG, "mqtt: discovery published (device_id=%s)", device_id);
}

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
        esp_mqtt_client_publish(event->client, s_topic_status, "online", 6, 1, 1);
        publish_discovery();
        publish_full_state();
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
