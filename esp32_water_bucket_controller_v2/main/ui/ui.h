#ifndef WB_UI_H
#define WB_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "priv.h"

#define UI_ROWS 8
#define UI_COLS 16

typedef enum {
    UI_PAGE_HOME = 0,
    UI_PAGE_PUMPS,
    UI_PAGE_SENSORS,
    UI_PAGE_LOGS,
    UI_PAGE_SETTINGS,
    UI_PAGE_COUNT
} ui_page_t;

typedef struct {
    ui_page_t page;
    uint8_t cursor;
    uint16_t scroll;
    bool menu_mode;
    uint8_t home_menu_cursor;
    uint8_t settings_contrast_idx;
} ui_state_t;

typedef struct {
    char rows[UI_ROWS][UI_COLS + 1];
    int invert_row;
} ui_frame_t;

void ui_log_init(void);
size_t ui_log_count(void);
void ui_log_get_recent(size_t index_from_newest, char *out, size_t out_len);

void ui_pages_init(ui_state_t *state);
void ui_pages_handle_input(ui_state_t *state, ui_input_event_t event);
void ui_pages_build_frame(const ui_state_t *state, ui_frame_t *frame);

esp_err_t ui_render_frame(const ui_frame_t *frame);

bool ui_runtime_wifi_connected(void);
bool ui_runtime_mqtt_connected(void);
uint32_t ui_runtime_uptime_s(void);
uint32_t ui_runtime_sensor_age_s(int idx);

#endif
