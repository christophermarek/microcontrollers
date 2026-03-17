#ifndef WB_UI_PAGES_INTERNAL_H
#define WB_UI_PAGES_INTERNAL_H

#include "ui.h"

extern const uint8_t g_ui_contrast_levels[4];

void ui_pages_set_line(char out[17], const char *text);
void ui_pages_set_linef(char out[17], const char *fmt, ...);
void ui_pages_header_title(ui_frame_t *f, const char *title);

void ui_page_build_home(const ui_state_t *state, ui_frame_t *frame);
void ui_page_build_pumps(const ui_state_t *state, ui_frame_t *frame);
void ui_page_build_sensors(const ui_state_t *state, ui_frame_t *frame);
void ui_page_build_logs(const ui_state_t *state, ui_frame_t *frame);
void ui_page_build_settings(const ui_state_t *state, ui_frame_t *frame);
void ui_settings_clamp_scroll(ui_state_t *s);

#endif
