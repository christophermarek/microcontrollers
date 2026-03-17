#include "ui_pages_internal.h"

void ui_page_build_home(const ui_state_t *state, ui_frame_t *frame)
{
    ui_pages_header_title_time(frame, "HOME");
    ui_pages_set_linef(frame->rows[1], "L1:%c L2:%c L3:%c",
                       s_level[0] ? 'D' : 'W',
                       s_level[1] ? 'D' : 'W',
                       s_level[2] ? 'D' : 'W');
    ui_pages_set_linef(frame->rows[2], "Pump:%s", s_current_pump < WB_NUM_PUMPS ? "ON" : "OFF");
    ui_pages_set_line(frame->rows[3], "1 Pumps");
    ui_pages_set_line(frame->rows[4], "2 Sensors");
    ui_pages_set_line(frame->rows[5], "3 Logs");
    ui_pages_set_line(frame->rows[6], "4 Settings");
    ui_pages_set_line(frame->rows[7], "");
    frame->invert_row = (int)state->cursor + 3;
}
