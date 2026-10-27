#include "test_all.h"
#include "pin_config.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "gpio";

void test_buttons_init(void)
{
    const gpio_num_t keys[] = { PIN_KEY_BOOT, PIN_KEY_DOWN, PIN_KEY_USER };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << keys[i]),
            .mode         = GPIO_MODE_INPUT,
            /* 原理图已外部上拉到 VCC，内部再上拉做兜底 */
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        gpio_config(&cfg);
    }
}

/* 返回 bit0=BOOT(GPIO9) bit1=DOWN(GPIO3) bit2=USER(GPIO2)，1 = 按下 */
uint8_t test_buttons_read(void)
{
    uint8_t m = 0;
    if (gpio_get_level(PIN_KEY_BOOT) == 0) m |= (1 << 0);
    if (gpio_get_level(PIN_KEY_DOWN) == 0) m |= (1 << 1);
    if (gpio_get_level(PIN_KEY_USER) == 0) m |= (1 << 2);
    return m;
}

/* 用 LEDC 在蜂鸣器脚上发 2kHz 方波。
 * 注意：Netlist 中 NS4150B 功放的输入来源不明确（疑似由 IO18 经 RC 网络送入），
 * 这里仅验证 IO18 能输出 PWM；功放/喇叭通路需自行确认。 */
void test_buzzer_beep(int ms)
{
    ledc_timer_config_t t = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 2000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t);

    ledc_channel_config_t c = {
        .gpio_num   = PIN_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 512,            /* 50% */
        .hpoint     = 0,
    };
    ledc_channel_config(&c);

    vTaskDelay(pdMS_TO_TICKS(ms));

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    gpio_reset_pin(PIN_BUZZER);
    ESP_LOGI(TAG, "buzzer beep %d ms done", ms);
}
