#include <stdio.h>
#include "ui_pages_internal.h"

void ui_page_build_pumps(const ui_state_t *state, ui_frame_t *frame)
{
    ui_pages_header_title_time(frame, "PUMPS");
    for (int i = 0; i < 7; i++) {
        char row[32];
        if (i == 0) {
            snprintf(row, sizeof(row), "1 Enable: %s", s_ui_pump_enabled ? "ON" : "OFF");
        } else {
            int pnum = i;
            uint8_t pidx = (uint8_t)(pnum - 1);
            int on = (s_current_pump < WB_NUM_PUMPS && s_current_pump == pidx);
            snprintf(row, sizeof(row), "%d Pump %d %s", i + 1, pnum, on ? "ON" : "OFF");
        }
        ui_pages_set_line(frame->rows[i + 1], row);
    }
    frame->invert_row = (int)state->cursor;
}
