#include <stdio.h>
#include "ui_pages_internal.h"

void ui_page_build_sensors(const ui_state_t *state, ui_frame_t *frame)
{
    ui_pages_header_title_time(frame, "SENSORS");
    ui_pages_set_linef(frame->rows[1], "1 L1:%s", s_level[0] ? "DRY" : "WATER");
    ui_pages_set_linef(frame->rows[2], "1 L2:%s", s_level[1] ? "DRY" : "WATER");
    ui_pages_set_linef(frame->rows[3], "1 L3:%s", s_level[2] ? "DRY" : "WATER");
    ui_pages_set_linef(frame->rows[4], "2 A1:%lus", (unsigned long)ui_runtime_sensor_age_s(0));
    ui_pages_set_linef(frame->rows[5], "3 A2:%lus", (unsigned long)ui_runtime_sensor_age_s(1));
    ui_pages_set_linef(frame->rows[6], "4 A3:%lus", (unsigned long)ui_runtime_sensor_age_s(2));
    ui_pages_set_linef(frame->rows[7], "5 %s", s_pumps_disabled ? "Pumps:LOCK" : "Pumps:READY");
    frame->invert_row = (int)state->cursor;
}
