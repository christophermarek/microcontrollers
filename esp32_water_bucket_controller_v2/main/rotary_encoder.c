#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define ENC_A GPIO_NUM_26
#define ENC_B GPIO_NUM_33
#define ENC_SW GPIO_NUM_27
#define ENC_EV_SW 0x80u

static const char *TAG = "wb_ui";
static const int8_t s_quad[16] = {
    0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0
};

static QueueHandle_t s_enc_q;
static uint8_t s_enc_last;
static int64_t s_sw_last_us;

static void IRAM_ATTR enc_isr(void *arg)
{
    (void)arg;
    uint8_t st = (uint8_t)((gpio_get_level(ENC_A) << 1) | gpio_get_level(ENC_B));
    BaseType_t hp = pdFALSE;
    if (s_enc_q && xQueueSendFromISR(s_enc_q, &st, &hp) != pdTRUE) {
    }
    if (hp) {
        portYIELD_FROM_ISR();
    }
}

static void IRAM_ATTR enc_sw_isr(void *arg)
{
    (void)arg;
    uint8_t ev = ENC_EV_SW;
    BaseType_t hp = pdFALSE;
    if (s_enc_q) {
        (void)xQueueSendFromISR(s_enc_q, &ev, &hp);
    }
    if (hp) {
        portYIELD_FROM_ISR();
    }
}

static void enc_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint8_t st;
        if (xQueueReceive(s_enc_q, &st, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (st == ENC_EV_SW) {
                vTaskDelay(pdMS_TO_TICKS(5));
                if (gpio_get_level(ENC_SW) == 0) {
                    int64_t now = esp_timer_get_time();
                    if (now - s_sw_last_us > 80000) {
                        s_sw_last_us = now;
                        ESP_LOGI(TAG, "enc: switch pressed");
                    }
                }
                continue;
            }
            int8_t d = s_quad[(s_enc_last << 2) | (st & 3u)];
            s_enc_last = (st & 3u);
            if (d > 0) {
                ESP_LOGI(TAG, "enc: rotate CW");
            } else if (d < 0) {
                ESP_LOGI(TAG, "enc: rotate CCW");
            }
        }
    }
}

void rotary_encoder_init(void)
{
    s_enc_q = xQueueCreate(48, sizeof(uint8_t));
    if (!s_enc_q) {
        ESP_LOGW(TAG, "enc: queue create failed");
        return;
    }
    gpio_config_t io = {0};
    io.pin_bit_mask = (1ULL << ENC_A) | (1ULL << ENC_B) | (1ULL << ENC_SW);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);
    s_enc_last = (uint8_t)((gpio_get_level(ENC_A) << 1) | gpio_get_level(ENC_B));
    s_sw_last_us = esp_timer_get_time();
    gpio_set_intr_type(ENC_A, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(ENC_B, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(ENC_SW, GPIO_INTR_NEGEDGE);
    esp_err_t ir = gpio_install_isr_service(0);
    if (ir != ESP_OK && ir != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "enc: gpio_install_isr_service %s", esp_err_to_name(ir));
    }
    gpio_isr_handler_add(ENC_A, enc_isr, NULL);
    gpio_isr_handler_add(ENC_B, enc_isr, NULL);
    gpio_isr_handler_add(ENC_SW, enc_sw_isr, NULL);
    if (xTaskCreate(enc_task, "enc_ui", 3072, NULL, 5, NULL) != pdPASS) {
        ESP_LOGW(TAG, "enc: task create failed");
    }
    ESP_LOGI(TAG, "enc: A=26 B=33 SW=27");
}
