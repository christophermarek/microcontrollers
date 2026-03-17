#include <stdio.h>
#include "ui_pages_internal.h"

void ui_page_build_pumps(const ui_state_t *state, ui_frame_t *frame)
{
    static const char *items[8] = {
        "Enable",
        "Pump 0",
        "Pump 1",
        "Pump 2",
        "Pump 3",
        "Pump 4",
        "Pump 5",
        "Pump OFF"
    };
    ui_pages_header_title(frame, "PUMPS");
    uint8_t item = state->cursor == 0 ? 0 : (uint8_t)(state->cursor - 1);
    uint8_t start = item > 5 ? (uint8_t)(item - 5) : 0;
    for (int i = 0; i < 7; i++) {
        uint8_t idx = (uint8_t)(start + (unsigned)i);
        if (idx < 8) {
            char row[32];
            int n = (int)idx + 1;
            if (idx == 0) {
                snprintf(row, sizeof(row), "%d %s:%s", n, items[idx], s_ui_pump_enabled ? "ON" : "OFF");
            } else if (idx == (uint8_t)(s_current_pump + 1)) {
                snprintf(row, sizeof(row), "%d>%s", n, items[idx]);
            } else if (idx == 7 && s_current_pump >= WB_NUM_PUMPS) {
                snprintf(row, sizeof(row), "%d>%s", n, items[idx]);
            } else {
                snprintf(row, sizeof(row), "%d %s", n, items[idx]);
            }
            ui_pages_set_line(frame->rows[i + 1], row);
        } else {
            ui_pages_set_line(frame->rows[i + 1], "");
        }
    }
    if (state->cursor == 0) {
        frame->invert_row = 0;
    } else {
        frame->invert_row = 1 + (int)item - (int)start;
    }
}
