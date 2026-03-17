/*
 * gpio.c - 74HC238 pump select (EN + 3 address lines); three level inputs (13/14/25).
 * EN low disables decoder outputs; EN high with A,B,C = binary index selects one of pumps 0..5.
 */

#include "driver/gpio.h"
#include "esp_log.h"
#include "priv.h"

static const char *TAG = "wb";

#define PIN_DEC_EN  GPIO_NUM_17
#define PIN_DEC_A   GPIO_NUM_18
#define PIN_DEC_B   GPIO_NUM_16
#define PIN_DEC_C   GPIO_NUM_19

static const gpio_num_t s_level_pins[WB_NUM_LEVELS] = {
    GPIO_NUM_13,
    GPIO_NUM_14,
    GPIO_NUM_25
};

static void dec_pin_out(gpio_num_t pin)
{
    gpio_reset_pin(pin);
    gpio_config_t io = {0};
    io.pin_bit_mask = (1ULL << pin);
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);
}

void gpio_init(void)
{
    dec_pin_out(PIN_DEC_EN);
    dec_pin_out(PIN_DEC_A);
    dec_pin_out(PIN_DEC_B);
    dec_pin_out(PIN_DEC_C);
    gpio_set_level(PIN_DEC_EN, 0);
    gpio_set_level(PIN_DEC_A, 0);
    gpio_set_level(PIN_DEC_B, 0);
    gpio_set_level(PIN_DEC_C, 0);
    for (size_t i = 0; i < WB_NUM_LEVELS; i++) {
        gpio_reset_pin(s_level_pins[i]);
        gpio_config_t io = {0};
        io.pin_bit_mask = (1ULL << s_level_pins[i]);
        io.mode = GPIO_MODE_INPUT;
        io.pull_up_en = GPIO_PULLUP_DISABLE;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&io);
    }
    ESP_LOGI(TAG, "gpio_init: decoder EN=17 A=18 B=16 C=19; levels 13,14,25");
}

int level_gpio_get(int i)
{
    if (i < 0 || i >= WB_NUM_LEVELS) {
        return 0;
    }
    return gpio_get_level(s_level_pins[i]);
}

void pump_decoder_apply(uint8_t index)
{
    if (index >= WB_NUM_PUMPS) {
        gpio_set_level(PIN_DEC_EN, 0);
        return;
    }
    gpio_set_level(PIN_DEC_A, (int)(index & 1u));
    gpio_set_level(PIN_DEC_B, (int)((index >> 1) & 1u));
    gpio_set_level(PIN_DEC_C, (int)((index >> 2) & 1u));
    gpio_set_level(PIN_DEC_EN, 1);
}
