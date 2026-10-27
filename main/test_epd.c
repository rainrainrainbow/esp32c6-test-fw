#include "test_all.h"
#include "pin_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "epd";

/*
 * FPC-24P 上未见硬件 SPI 的专用 MOSI/SCLK 网络，只有 GPIO4 / GPIO5 两根通用线，
 * 因此这里用软件模拟 SPI。
 * ⚠️ 本函数只验证控制线能否正常翻转 + BUSY 是否回到空闲，
 *    真正的刷屏初始化序列必须拿到屏幕驱动 IC 的 datasheet 后替换下面的 send()。
 */
static void bb_send(uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        gpio_set_level(PIN_EPD_SCLK, 0);
        gpio_set_level(PIN_EPD_MOSI, (b >> i) & 0x01);
        gpio_set_level(PIN_EPD_SCLK, 1);
    }
    gpio_set_level(PIN_EPD_SCLK, 0);
}

bool test_epd_probe(bool *busy_idle)
{
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << PIN_EPD_CS) | (1ULL << PIN_EPD_RST) |
                        (1ULL << PIN_EPD_SCLK) | (1ULL << PIN_EPD_MOSI),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&out);

    gpio_config_t in = {
        .pin_bit_mask = (1ULL << PIN_EPD_BUSY),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&in);

    gpio_set_level(PIN_EPD_CS, 1);
    gpio_set_level(PIN_EPD_SCLK, 0);
    gpio_set_level(PIN_EPD_MOSI, 0);

    /* 硬件复位时序 */
    gpio_set_level(PIN_EPD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_EPD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* 屏未初始化时 BUSY 通常应为低（空闲） */
    *busy_idle = (gpio_get_level(PIN_EPD_BUSY) == 0);

    /* 发送一个占位字节并翻转 CS，验证走线连通 */
    gpio_set_level(PIN_EPD_CS, 0);
    bb_send(0x00);
    gpio_set_level(PIN_EPD_CS, 1);

    ESP_LOGI(TAG, "EPD 控制线已翻转, BUSY=%d", gpio_get_level(PIN_EPD_BUSY));
    return true;
}
