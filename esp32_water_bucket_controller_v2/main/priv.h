/*
 * priv.h - Internal API and shared state for the water bucket controller.
 *
 * Used by main component: gpio, level, pump, mqtt, wifi, log_tcp, ota, ui_test, main.cpp.
 *
 * Threading: s_pump_mux (pump.c) protects pump and level state when accessed
 * from level timer (level_timer_cb -> read_levels -> set_pump) and MQTT
 * task (mqtt_event -> set_pump). Level state (s_level, s_pumps_disabled)
 * is defined in level.c; pump state (s_current_pump) in pump.c.
 *
 * s_mqtt_client is set once from app_main after esp_mqtt_client_init(); read
 * by MQTT handler and publish functions. No mutex for publish (single-threaded
 * per context).
 */

#ifndef PRIV_H
#define PRIV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WB_NUM_PUMPS  6
#define WB_NUM_LEVELS 3
#define WB_PUMP_OFF   6

extern SemaphoreHandle_t s_pump_mux;       /* guards pump/level state across timer and MQTT task */
extern uint8_t s_current_pump;
extern int s_level[WB_NUM_LEVELS];
extern bool s_pumps_disabled;             /* true when all three dry; blocks turn-on */
extern esp_mqtt_client_handle_t s_mqtt_client;
extern int s_last_level[WB_NUM_LEVELS];   /* previous read for change detection */
extern bool s_last_pumps_disabled;

void gpio_init(void);
int level_gpio_get(int i);
void pump_decoder_apply(uint8_t index);

void set_pump(uint8_t index);
void read_levels(void);
void publish_levels(void);
void publish_pump(void);
void publish_full_state(void);
void level_timer_cb(void *arg);
void wifi_init_blocking(void);
void log_tcp_init(void);
void mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *data);
void ota_check_rollback(void);
void ota_start_from_url(const char *url);
esp_err_t lcd_init(void);
void rotary_encoder_init(void);
void ui_test_init(void);

#ifdef __cplusplus
}
#endif

#endif
