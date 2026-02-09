/*
 * gpio.c - Shared GPIO init and low-level access for water bucket controller.
 *
 * Pump outputs: GPIO 16, 17, 18, 19. Level inputs: GPIO 32, 33, 35.
 * HIGH = pump on / level dry; LOW = pump off / level wet. No internal pull on levels.
 */

#include "driver/gpio.h"
#include "esp_log.h"
#include "priv.h"

static const char *TAG = "wb";

static const gpio_num_t s_pump_pins[WB_NUM_PUMPS] = {
    GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_19
};
static const gpio_num_t s_level_pins[WB_NUM_LEVELS] = {
    GPIO_NUM_32, GPIO_NUM_33, GPIO_NUM_35
};

void gpio_init(void)  /* outputs LOW at start; level pins input, no pull */
{
    ESP_LOGI(TAG, "gpio_init: configuring pump pins 16..19 as outputs, level pins 32,33,35 as inputs");
    for (size_t i = 0; i < WB_NUM_PUMPS; i++) {
        gpio_reset_pin(s_pump_pins[i]);
        gpio_config_t io = {0};
        io.pin_bit_mask = (1ULL << s_pump_pins[i]);
        io.mode = GPIO_MODE_OUTPUT;
        io.pull_up_en = GPIO_PULLUP_DISABLE;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&io);
        gpio_set_level(s_pump_pins[i], 0);
    }
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
    ESP_LOGI(TAG, "gpio_init: done; all pumps off, levels undetermined until first read");
}

int level_gpio_get(int i)  /* returns 0 or 1; bounds-checked */
{
    if (i < 0 || i >= WB_NUM_LEVELS) return 0;
    return gpio_get_level(s_level_pins[i]);
}

void pump_gpio_set(int i, int level)  /* single pin; caller enforces one-at-a-time */
{
    if (i < 0 || i >= WB_NUM_PUMPS) return;
    gpio_set_level(s_pump_pins[i], level ? 1 : 0);
}
