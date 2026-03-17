/*
 * UI runtime task: consumes rotary input events, polls controller state, updates UI logs,
 * and renders frames to the display at a steady cadence.
 */
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "ui.h"
#include "ui_tz.h"

typedef enum {
    UI_EVT_INPUT = 0
} ui_evt_type_t;

typedef struct {
    ui_evt_type_t type;
    ui_input_event_t input;
    int a;
    int b;
} ui_evt_t;

static const char *TAG = "wb_ui";

static QueueHandle_t s_ui_q;
static ui_state_t s_state;
static bool s_wifi_connected;
static bool s_mqtt_connected;
static int64_t s_sensor_change_us[WB_NUM_LEVELS];
static int s_prev_level[WB_NUM_LEVELS];
static bool s_prev_disabled;
static uint8_t s_prev_pump;
static bool s_prev_ui_enabled;

static void ui_rotary_cb(rotary_event_t event, void *ctx)
{
    (void)ctx;
    switch (event) {
    case ROTARY_EVENT_CW:
        ui_post_input(UI_INPUT_ROTATE_CCW);
        break;
    case ROTARY_EVENT_CCW:
        ui_post_input(UI_INPUT_ROTATE_CW);
        break;
    case ROTARY_EVENT_PRESS_SHORT:
        ui_post_input(UI_INPUT_PRESS_SHORT);
        break;
    case ROTARY_EVENT_PRESS_LONG:
        ui_post_input(UI_INPUT_PRESS_LONG);
        break;
    default:
        break;
    }
}

static void ui_push_event(const ui_evt_t *e)
{
    if (s_ui_q == NULL || e == NULL) {
        return;
    }
    (void)xQueueSend(s_ui_q, e, 0);
}

void ui_post_input(ui_input_event_t event)
{
    ui_evt_t e = {
        .type = UI_EVT_INPUT,
        .input = event,
        .a = 0,
        .b = 0
    };
    ui_push_event(&e);
}

bool ui_runtime_wifi_connected(void)
{
    return s_wifi_connected;
}

bool ui_runtime_mqtt_connected(void)
{
    return s_mqtt_connected;
}

uint32_t ui_runtime_uptime_s(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000000LL);
}

uint32_t ui_runtime_sensor_age_s(int idx)
{
    if (idx < 0 || idx >= WB_NUM_LEVELS) {
        return 0;
    }
    int64_t now = esp_timer_get_time();
    int64_t dt = now - s_sensor_change_us[idx];
    if (dt < 0) {
        dt = 0;
    }
    return (uint32_t)(dt / 1000000LL);
}

static void ui_poll_runtime(void)
{
    bool wifi_now = s_wifi_connected_state;
    if (wifi_now != s_wifi_connected) {
        s_wifi_connected = wifi_now;
        ui_log_event(s_wifi_connected ? "WiFi connected" : "WiFi disconnected");
    }
    bool mqtt_now = false;
    mqtt_now = s_mqtt_connected_state;
    if (mqtt_now != s_mqtt_connected) {
        s_mqtt_connected = mqtt_now;
        ui_log_event(s_mqtt_connected ? "MQTT connected" : "MQTT disconnected");
    }
    for (int i = 0; i < WB_NUM_LEVELS; i++) {
        if (s_prev_level[i] != s_level[i]) {
            s_prev_level[i] = s_level[i];
            s_sensor_change_us[i] = esp_timer_get_time();
            ui_log_eventf("L%d %s", i + 1, s_level[i] ? "dry" : "water");
        }
    }
    if (s_prev_disabled != s_pumps_disabled) {
        s_prev_disabled = s_pumps_disabled;
        ui_log_event(s_pumps_disabled ? "Safety dry lock" : "Safety ready");
    }
    if (s_prev_pump != s_current_pump) {
        s_prev_pump = s_current_pump;
        if (s_current_pump >= WB_NUM_PUMPS) {
            ui_log_event("Pump off");
        } else {
            ui_log_eventf("Pump %u active", (unsigned)s_current_pump);
        }
    }
    if (s_prev_ui_enabled != s_ui_pump_enabled) {
        s_prev_ui_enabled = s_ui_pump_enabled;
        ui_log_eventf("UI pump %s", s_ui_pump_enabled ? "enabled" : "disabled");
    }
}

static const char *ui_input_tag(ui_input_event_t ev)
{
    switch (ev) {
    case UI_INPUT_ROTATE_CW: return "CW";
    case UI_INPUT_ROTATE_CCW: return "CCW";
    case UI_INPUT_PRESS_SHORT: return "SHORT";
    case UI_INPUT_PRESS_LONG: return "LONG";
    default: return "?";
    }
}

static void ui_trace_state(const char *in_tag)
{
    ESP_LOGI(TAG, "ui state in=%s page=%d menu=%d cur=%u scroll=%u",
             in_tag, (int)s_state.page, (int)s_state.menu_mode,
             (unsigned)s_state.cursor, (unsigned)s_state.scroll);
}

static void ui_handle_event(const ui_evt_t *e)
{
    if (e->type == UI_EVT_INPUT) {
        ui_pages_handle_input(&s_state, e->input);
        ui_trace_state(ui_input_tag(e->input));
        return;
    }
}

static void ui_task(void *arg)
{
    (void)arg;
    ui_frame_t frame;
    for (;;) {
        ui_evt_t ev;
        if (xQueueReceive(s_ui_q, &ev, pdMS_TO_TICKS(200)) == pdTRUE) {
            ui_handle_event(&ev);
        }
        ui_poll_runtime();
        ui_pages_build_frame(&s_state, &frame);
        (void)ui_render_frame(&frame);
    }
}

void ui_init(void)
{
    if (s_ui_q != NULL) {
        return;
    }
    memset(s_sensor_change_us, 0, sizeof(s_sensor_change_us));
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < WB_NUM_LEVELS; i++) {
        s_sensor_change_us[i] = now;
        s_prev_level[i] = s_level[i];
    }
    s_prev_disabled = s_pumps_disabled;
    s_prev_pump = s_current_pump;
    s_prev_ui_enabled = s_ui_pump_enabled;
    s_ui_q = xQueueCreate(96, sizeof(ui_evt_t));
    if (s_ui_q == NULL) {
        ESP_LOGE(TAG, "ui queue create failed");
        return;
    }
    ui_tz_init();
    ui_log_init();
    ui_pages_init(&s_state);
    ui_log_event("UI init");
    ui_trace_state("init");
    rotary_encoder_set_callback(ui_rotary_cb, NULL);
    if (xTaskCreate(ui_task, "ui_task", 6144, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "ui task create failed");
    }
}
