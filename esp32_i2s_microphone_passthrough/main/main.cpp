#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <algorithm>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s_std.h"

static const char *TAG = "i2s_passthrough";

static constexpr gpio_num_t PIN_BCLK   = GPIO_NUM_26;
static constexpr gpio_num_t PIN_WS     = GPIO_NUM_25;
static constexpr gpio_num_t PIN_MIC_SD = GPIO_NUM_33;  // INMP441 SD (data out)
static constexpr gpio_num_t PIN_AMP_DIN= GPIO_NUM_22;  // MAX98357 DIN (data in)

static constexpr int SAMPLE_RATE = 16000;

// I2S frames: stereo slots (L,R)
static constexpr int FRAMES = 256;
static constexpr int WORDS_PER_FRAME = 2;
static constexpr int BUF_WORDS = FRAMES * WORDS_PER_FRAME;

// Simple clip helper
static inline int16_t clip16(int32_t x) {
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return (int16_t)x;
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Mic->Speaker passthrough (INMP441 RX, MAX98357 TX)");
    ESP_LOGI(TAG, "BCLK=%d WS=%d MIC_SD(DIN)=%d AMP_DIN(DOUT)=%d SR=%d",
             (int)PIN_BCLK, (int)PIN_WS, (int)PIN_MIC_SD, (int)PIN_AMP_DIN, SAMPLE_RATE);

    i2s_chan_handle_t tx_chan = nullptr;
    i2s_chan_handle_t rx_chan = nullptr;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = 8;
    chan_cfg.dma_frame_num = FRAMES;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan));

    // Use standard Philips I2S, 32-bit stereo slots for both directions (shared clocks).
    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE);

    i2s_std_slot_config_t slot_cfg =
        I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);

    // RX GPIO (mic)
    i2s_std_gpio_config_t rx_gpio{};
    rx_gpio.mclk = I2S_GPIO_UNUSED;
    rx_gpio.bclk = PIN_BCLK;
    rx_gpio.ws   = PIN_WS;
    rx_gpio.dout = I2S_GPIO_UNUSED;
    rx_gpio.din  = PIN_MIC_SD;
    rx_gpio.invert_flags = {};

    // TX GPIO (amp)
    i2s_std_gpio_config_t tx_gpio{};
    tx_gpio.mclk = I2S_GPIO_UNUSED;
    tx_gpio.bclk = PIN_BCLK;
    tx_gpio.ws   = PIN_WS;
    tx_gpio.dout = PIN_AMP_DIN;
    tx_gpio.din  = I2S_GPIO_UNUSED;
    tx_gpio.invert_flags = {};

    i2s_std_config_t rx_cfg{};
    rx_cfg.clk_cfg  = clk_cfg;
    rx_cfg.slot_cfg = slot_cfg;
    rx_cfg.gpio_cfg = rx_gpio;

    i2s_std_config_t tx_cfg{};
    tx_cfg.clk_cfg  = clk_cfg;
    tx_cfg.slot_cfg = slot_cfg;
    tx_cfg.gpio_cfg = tx_gpio;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_cfg));

    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    static int32_t rx_buf[BUF_WORDS];
    static int32_t tx_buf[BUF_WORDS];

    // Software gain (start modest; raise if too quiet)
    // 1.0 = unity, 2.0 = +6dB-ish, 4.0 = +12dB-ish (clip risk)
    const float gain = 4.0f;

    while (true) {
        size_t rx_bytes = 0;
        esp_err_t r = i2s_channel_read(rx_chan, rx_buf, sizeof(rx_buf), &rx_bytes, portMAX_DELAY);
        if (r != ESP_OK || rx_bytes == 0) {
            ESP_LOGW(TAG, "read err=%d rx_bytes=%u", (int)r, (unsigned)rx_bytes);
            continue;
        }

        const int frames_read = (int)(rx_bytes / (sizeof(int32_t) * WORDS_PER_FRAME));

        int32_t max_abs_16 = 0;
        int64_t sum_abs_16 = 0;

        for (int i = 0; i < frames_read; i++) {
            // INMP441 with L/R tied HIGH -> data in RIGHT slot typically.
            int32_t right32 = rx_buf[i * 2 + 1];

            // Convert INMP441 32-bit word -> signed 24-bit sample (common INMP441 packing)
            // right32: valid audio usually in bits [31:8], bottom 8 bits are 0
            int32_t s24 = (right32 >> 8);          // now approx signed 24-bit in low bits

            // Apply gain in 24-bit domain
            float f = (float)s24 * gain;

            // Convert 24-bit -> 16-bit (shift down), then clip
            int32_t s16_32 = (int32_t)(f / 256.0f); // 24->16 scale (>>8 equivalent)
            int16_t s16 = clip16(s16_32);

            int32_t a = std::abs((int32_t)s16);
            max_abs_16 = std::max(max_abs_16, a);
            sum_abs_16 += a;

            // Pack 16-bit sample into TOP 16 bits of a 32-bit I2S slot:
            // This is the safest “MAX98357 will definitely see it” packing.
            int32_t out32 = ((int32_t)s16) << 16;

            // Feed same sample to both channels
            tx_buf[i * 2 + 0] = out32;
            tx_buf[i * 2 + 1] = out32;
        }

        size_t tx_bytes = 0;
        esp_err_t w = i2s_channel_write(
            tx_chan,
            tx_buf,
            frames_read * WORDS_PER_FRAME * sizeof(int32_t),
            &tx_bytes,
            portMAX_DELAY
        );

        if (w != ESP_OK) {
            ESP_LOGW(TAG, "write err=%d tx_bytes=%u", (int)w, (unsigned)tx_bytes);
        }

        // Log levels ~2x per second so you can see if sound energy is present
        static int ctr = 0;
        if ((ctr++ % 8) == 0) {
            int avg_abs_16 = (frames_read > 0) ? (int)(sum_abs_16 / frames_read) : 0;
            ESP_LOGI(TAG, "frames=%d tx_bytes=%u maxAbs16=%d avgAbs16=%d",
                     frames_read, (unsigned)tx_bytes, max_abs_16, avg_abs_16);
        }
    }
}
