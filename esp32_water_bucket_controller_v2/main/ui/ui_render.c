#include "ui.h"

esp_err_t ui_render_frame(const ui_frame_t *frame)
{
    if (frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return lcd_draw_rows(frame->rows, frame->invert_row);
}
