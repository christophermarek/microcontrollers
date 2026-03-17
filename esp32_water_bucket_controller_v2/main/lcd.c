#include <string.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_ssd1306.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define OLED_SDA GPIO_NUM_22
#define OLED_SCL GPIO_NUM_32
#define OLED_ADDR 0x3C
#define OLED_H 128
#define OLED_V 64
#define GLYPH_CELL_W 7
#define LINE_H 8

static const char *TAG = "wb_ui";

static esp_lcd_panel_handle_t s_panel;
static uint8_t s_fb[OLED_H * OLED_V / 8];
static int s_marquee_px;

static const uint8_t GLYPH_SPACE[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t GLYPH_COLON[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
static const uint8_t GLYPH_0[5] = {0x3E, 0x51, 0x49, 0x49, 0x3E};
static const uint8_t GLYPH_1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
static const uint8_t GLYPH_2[5] = {0x62, 0x51, 0x49, 0x49, 0x46};
static const uint8_t GLYPH_3[5] = {0x22, 0x49, 0x49, 0x49, 0x36};
static const uint8_t GLYPH_4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
static const uint8_t GLYPH_5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
static const uint8_t GLYPH_6[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
static const uint8_t GLYPH_7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
static const uint8_t GLYPH_8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
static const uint8_t GLYPH_9[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};
static const uint8_t GLYPH_H[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
static const uint8_t GLYPH_E[5] = {0x38, 0x54, 0x54, 0x54, 0x18};
static const uint8_t GLYPH_L[5] = {0x00, 0x00, 0x7F, 0x40, 0x40};
static const uint8_t GLYPH_o[5] = {0x1C, 0x22, 0x22, 0x22, 0x1C};
static const uint8_t GLYPH_O[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
static const uint8_t GLYPH_Lcap[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
static const uint8_t GLYPH_Ecap[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
static const uint8_t GLYPH_V[5] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
static const uint8_t GLYPH_W[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
static const uint8_t GLYPH_R[5] = {0x7C, 0x08, 0x04, 0x04, 0x08};
static const uint8_t GLYPH_D[5] = {0x38, 0x44, 0x44, 0x48, 0x7F};
static const uint8_t GLYPH_T[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
static const uint8_t GLYPH_S[5] = {0x46, 0x49, 0x49, 0x29, 0x1E};
static const uint8_t GLYPH_I[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
static const uint8_t GLYPH_N[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
static const uint8_t GLYPH_G[5] = {0x3E, 0x41, 0x49, 0x49, 0x7A};
static const uint8_t GLYPH_A[5] = {0x20, 0x54, 0x54, 0x54, 0x78};
static const uint8_t GLYPH_M[5] = {0x7F, 0x02, 0x04, 0x02, 0x7F};
static const uint8_t GLYPH_P[5] = {0x7F, 0x08, 0x14, 0x14, 0x08};
static const uint8_t GLYPH_U[5] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
static const uint8_t GLYPH_B[5] = {0x7F, 0x49, 0x49, 0x49, 0x36};
static const uint8_t GLYPH_K[5] = {0x7F, 0x08, 0x14, 0x22, 0x41};
static const uint8_t GLYPH_C[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
static const uint8_t GLYPH_F[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
static const uint8_t GLYPH_Y[5] = {0x03, 0x04, 0x78, 0x04, 0x03};
static const uint8_t GLYPH_STAR[5] = {0x22, 0x14, 0x7F, 0x14, 0x22};
static const uint8_t GLYPH_DASH[5] = {0x08, 0x08, 0x08, 0x08, 0x08};

static const uint8_t *glyph(char c)
{
    switch (c) {
    case ' ':
        return GLYPH_SPACE;
    case ':':
        return GLYPH_COLON;
    case '0':
        return GLYPH_0;
    case '1':
        return GLYPH_1;
    case '2':
        return GLYPH_2;
    case '3':
        return GLYPH_3;
    case '4':
        return GLYPH_4;
    case '5':
        return GLYPH_5;
    case '6':
        return GLYPH_6;
    case '7':
        return GLYPH_7;
    case '8':
        return GLYPH_8;
    case '9':
        return GLYPH_9;
    case 'H':
        return GLYPH_H;
    case 'e':
        return GLYPH_E;
    case 'l':
        return GLYPH_L;
    case 'o':
        return GLYPH_o;
    case 'O':
        return GLYPH_O;
    case 'L':
        return GLYPH_Lcap;
    case 'E':
        return GLYPH_Ecap;
    case 'V':
        return GLYPH_V;
    case 'W':
        return GLYPH_W;
    case 'r':
        return GLYPH_R;
    case 'd':
        return GLYPH_D;
    case 'T':
        return GLYPH_T;
    case 'S':
        return GLYPH_S;
    case 'I':
        return GLYPH_I;
    case 'N':
        return GLYPH_N;
    case 'G':
        return GLYPH_G;
    case 'A':
        return GLYPH_A;
    case 'M':
        return GLYPH_M;
    case 'P':
        return GLYPH_P;
    case 'U':
        return GLYPH_U;
    case 'B':
        return GLYPH_B;
    case 'K':
        return GLYPH_K;
    case 'C':
        return GLYPH_C;
    case 'F':
        return GLYPH_F;
    case 'Y':
        return GLYPH_Y;
    case '*':
        return GLYPH_STAR;
    case '-':
        return GLYPH_DASH;
    default:
        return GLYPH_SPACE;
    }
}

static void fb_set(uint8_t *fb, int x, int y)
{
    if (x < 0 || x >= OLED_H || y < 0 || y >= OLED_V) {
        return;
    }
    fb[(size_t)x + (((size_t)y) >> 3) * OLED_H] |= (uint8_t)(1u << (y & 7));
}

static void draw_char_5x7(uint8_t *fb, int x, int y, char c)
{
    const uint8_t *g = glyph(c);
    for (int col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (int row = 0; row < 7; row++) {
            if ((bits >> row) & 1u) {
                fb_set(fb, x + col, y + row);
            }
        }
    }
}

static void draw_text_at(uint8_t *fb, int x, int y, const char *text)
{
    for (size_t i = 0; text[i] != '\0'; i++) {
        int cx = x + (int)i * GLYPH_CELL_W;
        if (cx >= OLED_H) {
            break;
        }
        if (cx + 5 < 0) {
            continue;
        }
        draw_char_5x7(fb, cx, y, text[i]);
    }
}

static void draw_marquee_loop(uint8_t *fb, int y, const char *segment, int scroll_px)
{
    if (s_marquee_px <= 0) {
        return;
    }
    int off = scroll_px % s_marquee_px;
    if (off < 0) {
        off += s_marquee_px;
    }
    draw_text_at(fb, -off, y, segment);
    draw_text_at(fb, -off + s_marquee_px, y, segment);
}

static const char s_marquee_seg[] =
    " *** SIDE SCROLL / ANIMATION TEST *** PUMPS LEVELS MQTT *** ";

static void lcd_test_frame(int scroll_px)
{
    memset(s_fb, 0, sizeof(s_fb));
    draw_text_at(s_fb, 0, 0 * LINE_H, "1: Hello");
    draw_text_at(s_fb, 0, 1 * LINE_H, "2: World");
    draw_marquee_loop(s_fb, 2 * LINE_H, s_marquee_seg, scroll_px);
    draw_text_at(s_fb, 0, 3 * LINE_H, "4: STATUS OK");
    draw_text_at(s_fb, 0, 4 * LINE_H, "5: PUMP NET");
    draw_text_at(s_fb, 0, 5 * LINE_H, "6: LEVEL HA");
    draw_text_at(s_fb, 0, 6 * LINE_H, "7: WIFI OTA");
    draw_text_at(s_fb, 0, 7 * LINE_H, "8: --------");
    (void)esp_lcd_panel_draw_bitmap(s_panel, 0, 0, OLED_H, OLED_V, s_fb);
}

static void lcd_anim_task(void *arg)
{
    (void)arg;
    int scroll = 0;
    for (;;) {
        lcd_test_frame(scroll);
        scroll += 2;
        if (s_marquee_px > 0 && scroll >= s_marquee_px) {
            scroll = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

esp_err_t lcd_init(void)
{
    s_marquee_px = (int)(sizeof(s_marquee_seg) - 1u) * GLYPH_CELL_W;

    i2c_master_bus_handle_t bus = NULL;
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = OLED_SDA,
        .scl_io_num = OLED_SCL,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t e = i2c_new_master_bus(&bus_cfg, &bus);
    if (e != ESP_OK) {
        return e;
    }
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = {
        .dev_addr = OLED_ADDR,
        .scl_speed_hz = 400000,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_bit_offset = 6,
    };
    e = esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io);
    if (e != ESP_OK) {
        return e;
    }
    esp_lcd_panel_ssd1306_config_t ssd_cfg = {.height = OLED_V};
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,
        .bits_per_pixel = 1,
        .vendor_config = &ssd_cfg,
    };
    e = esp_lcd_new_panel_ssd1306(io, &panel_cfg, &s_panel);
    if (e != ESP_OK) {
        return e;
    }
    e = esp_lcd_panel_reset(s_panel);
    if (e != ESP_OK) {
        return e;
    }
    e = esp_lcd_panel_init(s_panel);
    if (e != ESP_OK) {
        return e;
    }
    e = esp_lcd_panel_mirror(s_panel, true, true);
    if (e != ESP_OK) {
        return e;
    }
    e = esp_lcd_panel_disp_on_off(s_panel, true);
    if (e != ESP_OK) {
        return e;
    }

    lcd_test_frame(0);
    if (xTaskCreate(lcd_anim_task, "lcd_anim", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGW(TAG, "lcd: anim task create failed");
    }
    ESP_LOGI(TAG, "lcd: SSD1306 ok multiline+marquee (SDA=22 SCL=32)");
    return ESP_OK;
}
