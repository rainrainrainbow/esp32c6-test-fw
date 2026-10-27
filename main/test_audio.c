#include "test_all.h"
#include "pin_config.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "mic";

/*
 * ICS-43434 是 I2S 输出的数字麦克风（非 PDM），24-bit 数据在 32-bit 帧内 MSB 对齐。
 */
bool test_i2s_mic(int32_t *peak, int *nonzero_samples)
{
    i2s_chan_handle_t rx = NULL;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = 8;
    chan_cfg.dma_frame_num = 256;

    if (i2s_new_channel(&chan_cfg, NULL, &rx) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed");
        return false;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(I2S_MIC_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                    I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = GPIO_NUM_NC,
            .bclk = PIN_I2S_BCLK,
            .ws   = PIN_I2S_WS,
            .dout = GPIO_NUM_NC,
            .din  = PIN_I2S_DIN,
        },
    };
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    if (i2s_channel_init_std_mode(rx, &std_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "init std mode failed");
        i2s_del_channel(rx);
        return false;
    }
    if (i2s_channel_enable(rx) != ESP_OK) {
        ESP_LOGE(TAG, "enable failed");
        i2s_del_channel(rx);
        return false;
    }

    /* 丢弃第一帧（防止上电瞬态） */
    int32_t junk[256];
    size_t  got = 0;
    i2s_channel_read(rx, junk, sizeof(junk), &got, pdMS_TO_TICKS(200));

    int32_t buf[512];
    size_t  bytes_read = 0;
    esp_err_t err = i2s_channel_read(rx, buf, sizeof(buf), &bytes_read,
                                     pdMS_TO_TICKS(500));

    i2s_channel_disable(rx);
    i2s_del_channel(rx);

    if (err != ESP_OK || bytes_read == 0) {
        ESP_LOGE(TAG, "read failed: %s", esp_err_to_name(err));
        return false;
    }

    int32_t mx = 0;
    int     nz = 0;
    size_t  n  = bytes_read / sizeof(int32_t);
    for (size_t i = 0; i < n; i++) {
        int32_t s = buf[i] >> 8;          /* 取高 24 位 */
        if (s < 0) s = -s;
        if (s > mx) mx = s;
        if (s > 100) nz++;
    }

    *peak             = mx;
    *nonzero_samples  = nz;
    if (nz == 0) {
        ESP_LOGW(TAG, "all samples silent —— 检查 BCLK/WS/SD 引脚与供电");
    }
    return true;
}
