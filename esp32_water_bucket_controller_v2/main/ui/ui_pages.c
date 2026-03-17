#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ui_pages_internal.h"
#include "ui_tz.h"

#define STZ_MAX 13
#define STZ_TZ 2

void ui_pages_header_title(ui_frame_t *f, const char *title)
{
    char line[17];
    memset(line, ' ', 16);
    if (title == NULL) {
        title = "";
    }
    size_t n = strlen(title);
    if (n > 16) {
        n = 16;
    }
    memcpy(line, title, n);
    line[16] = '\0';
    ui_pages_set_line(f->rows[0], line);
}

void ui_pages_header_title_time(ui_frame_t *f, const char *title)
{
    char line[17];
    char tmb[10];
    memset(line, ' ', 16);
    if (title == NULL) {
        title = "";
    }
    size_t n = strlen(title);
    if (n > 7) {
        n = 7;
    }
    memcpy(line, title, n);
    time_t t = time(NULL);
    if (t < 1700000000) {
        memcpy(line + 8, "--:--:--", 8);
    } else {
        struct tm tm;
        localtime_r(&t, &tm);
        snprintf(tmb, sizeof(tmb), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
        size_t tl = strlen(tmb);
        if (tl > 8) {
            tl = 8;
        }
        memcpy(line + 8, tmb, tl);
    }
    line[16] = '\0';
    ui_pages_set_line(f->rows[0], line);
}

static uint32_t ui_logs_max_scroll(void)
{
    size_t nseg = ui_logs_wrap_segment_count();
    if (nseg <= 6) {
        return 0;
    }
    return (uint32_t)(nseg - 6);
}

const uint8_t g_ui_contrast_levels[4] = {0x40, 0x80, 0xC0, 0xFF};

void ui_pages_set_line(char out[17], const char *text)
{
    size_t n = strlen(text);
    if (n > 16) {
        n = 16;
    }
    memset(out, ' ', 16);
    memcpy(out, text, n);
    out[16] = '\0';
}

void ui_pages_set_linef(char out[17], const char *fmt, ...)
{
    char tmp[48];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    ui_pages_set_line(out, tmp);
}

void ui_pages_init(ui_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->page = UI_PAGE_HOME;
    state->menu_mode = true;
    state->settings_contrast_idx = 3;
}

static void go_home_menu(ui_state_t *s)
{
    s->page = UI_PAGE_HOME;
    s->cursor = s->home_menu_cursor % 4;
    s->scroll = 0;
    s->menu_mode = true;
}

void ui_pages_handle_input(ui_state_t *s, ui_input_event_t event)
{
    if (event == UI_INPUT_PRESS_LONG) {
        go_home_menu(s);
        return;
    }
    if (s->page == UI_PAGE_HOME && s->menu_mode) {
        if (event == UI_INPUT_ROTATE_CW) {
            s->cursor = (uint8_t)((s->cursor + 1) % 4);
            s->home_menu_cursor = s->cursor;
        } else if (event == UI_INPUT_ROTATE_CCW) {
            s->cursor = (uint8_t)((s->cursor + 3) % 4);
            s->home_menu_cursor = s->cursor;
        } else if (event == UI_INPUT_PRESS_SHORT) {
            s->home_menu_cursor = s->cursor;
            s->page = (ui_page_t)(s->cursor + 1);
            s->scroll = 0;
            s->menu_mode = false;
            s->cursor = 1;
        }
        return;
    }
    if (event == UI_INPUT_ROTATE_CW) {
        if (s->page == UI_PAGE_PUMPS) {
            s->cursor = (uint8_t)((s->cursor + 1) % 8);
        } else if (s->page == UI_PAGE_SENSORS) {
            s->cursor = (uint8_t)((s->cursor + 1) % 8);
        } else if (s->page == UI_PAGE_SETTINGS) {
            if (s->cursor == 0) {
                s->cursor = 1;
            } else if (s->cursor >= STZ_MAX) {
                s->cursor = 0;
            } else {
                s->cursor++;
            }
            ui_settings_clamp_scroll(s);
        } else if (s->page == UI_PAGE_LOGS) {
            if (s->cursor == 0) {
                s->cursor = 1;
            } else {
                uint32_t mx = ui_logs_max_scroll();
                if (mx > 0) {
                    s->scroll = (uint16_t)((s->scroll + 1) % (mx + 1));
                }
            }
        }
        return;
    }
    if (event == UI_INPUT_ROTATE_CCW) {
        if (s->page == UI_PAGE_PUMPS) {
            s->cursor = (uint8_t)((s->cursor + 7) % 8);
        } else if (s->page == UI_PAGE_SENSORS) {
            s->cursor = (uint8_t)((s->cursor + 7) % 8);
        } else if (s->page == UI_PAGE_SETTINGS) {
            if (s->cursor == 0) {
                s->cursor = STZ_MAX;
            } else {
                s->cursor--;
            }
            ui_settings_clamp_scroll(s);
        } else if (s->page == UI_PAGE_LOGS) {
            if (s->cursor == 1) {
                if (s->scroll > 0) {
                    s->scroll = (uint16_t)(s->scroll - 1);
                } else {
                    s->cursor = 0;
                }
            } else {
                s->cursor = 1;
            }
        }
        return;
    }
    if (event != UI_INPUT_PRESS_SHORT) {
        return;
    }
    if (s->page == UI_PAGE_PUMPS) {
        if (s->cursor == 0) {
            go_home_menu(s);
            return;
        }
        uint8_t item = (uint8_t)(s->cursor - 1);
        if (item == 0) {
            set_ui_pump_enabled(!s_ui_pump_enabled);
            ui_log_eventf("UI pump %s", s_ui_pump_enabled ? "enabled" : "disabled");
            return;
        }
        uint8_t pidx = (uint8_t)(item - 1);
        if (pidx < WB_NUM_PUMPS) {
            if (s_current_pump == pidx) {
                set_pump(WB_PUMP_OFF);
            } else {
                set_pump(pidx);
            }
        }
        return;
    }
    if (s->page == UI_PAGE_SENSORS) {
        if (s->cursor == 0) {
            go_home_menu(s);
        }
        return;
    }
    if (s->page == UI_PAGE_LOGS) {
        if (s->cursor == 0) {
            go_home_menu(s);
        }
        return;
    }
    if (s->page == UI_PAGE_SETTINGS) {
        if (s->cursor == 0) {
            go_home_menu(s);
        } else if (s->cursor == 1) {
            s->settings_contrast_idx = (uint8_t)((s->settings_contrast_idx + 1) % 4);
            (void)lcd_set_contrast(g_ui_contrast_levels[s->settings_contrast_idx]);
            ui_log_eventf("UI contrast %u", (unsigned)g_ui_contrast_levels[s->settings_contrast_idx]);
        } else if (s->cursor == STZ_TZ) {
            ui_tz_set((uint8_t)((ui_tz_get() + 1) % UI_TZ_COUNT));
        } else if (s->cursor < STZ_MAX) {
            s->cursor++;
        } else {
            s->cursor = 1;
        }
        ui_settings_clamp_scroll(s);
    }
}

void ui_pages_build_frame(const ui_state_t *state, ui_frame_t *frame)
{
    memset(frame, 0, sizeof(*frame));
    frame->invert_row = -1;
    switch (state->page) {
    case UI_PAGE_HOME:
        ui_page_build_home(state, frame);
        break;
    case UI_PAGE_PUMPS:
        ui_page_build_pumps(state, frame);
        break;
    case UI_PAGE_SENSORS:
        ui_page_build_sensors(state, frame);
        break;
    case UI_PAGE_LOGS:
        ui_page_build_logs(state, frame);
        break;
    case UI_PAGE_SETTINGS:
        ui_page_build_settings(state, frame);
        break;
    default:
        ui_page_build_home(state, frame);
        break;
    }
}
