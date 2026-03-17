#include "esp_log.h"
#include "priv.h"

static const char *TAG = "wb_ui";

void ui_test_init(void)
{
    esp_err_t e = lcd_init();
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "lcd: init failed %s", esp_err_to_name(e));
    }
    rotary_encoder_init();
}
