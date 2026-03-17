#include <string.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "ssd1306.h"

#define OLED_SDA GPIO_NUM_22
#define OLED_SCL GPIO_NUM_32
#define OLED_ADDR 0x3C

static const char *TAG = "wb_ui";

static ssd1306_handle_t s_lcd;
static i2c_master_bus_handle_t s_i2c_bus;

static void line16(char out[17], const char *text)
{
    size_t n = strlen(text);
    if (n > 16) {
        n = 16;
    }
    memset(out, ' ', 16);
    memcpy(out, text, n);
    out[16] = '\0';
}

esp_err_t lcd_init(void)
{
    if (s_lcd != NULL) {
        return ESP_OK;
    }
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = OLED_SDA,
        .scl_io_num = OLED_SCL,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t e = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
    if (e != ESP_OK) {
        return e;
    }
    ssd1306_config_t cfg = I2C_SSD1306_128x64_CONFIG_DEFAULT;
    cfg.i2c_address = OLED_ADDR;
    cfg.i2c_clock_speed = 400000;
    cfg.flip_enabled = true;
    e = ssd1306_init(s_i2c_bus, &cfg, &s_lcd);
    if (e != ESP_OK) {
        return e;
    }
    e = ssd1306_clear_display(s_lcd, false);
    if (e != ESP_OK) {
        return e;
    }
    e = ssd1306_set_contrast(s_lcd, 0xFF);
    if (e != ESP_OK) {
        return e;
    }
    ESP_LOGI(TAG, "lcd: ready");
    return ESP_OK;
}

esp_err_t lcd_draw_rows(const char rows[8][17], int invert_row)
{
    if (s_lcd == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    for (int i = 0; i < 8; i++) {
        char buf[17];
        line16(buf, rows[i]);
        esp_err_t e = ssd1306_display_text(s_lcd, (uint8_t)i, buf, i == invert_row);
        if (e != ESP_OK) {
            return e;
        }
    }
    return ESP_OK;
}

esp_err_t lcd_set_flip(bool enabled)
{
    if (s_lcd == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_lcd->dev_config.flip_enabled = enabled;
    return ssd1306_clear_display(s_lcd, false);
}

esp_err_t lcd_set_contrast(uint8_t contrast)
{
    if (s_lcd == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return ssd1306_set_contrast(s_lcd, contrast);
}
