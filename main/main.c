#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_timer.h"
#include "test_all.h"
#include "pin_config.h"

static const char *TAG = "hwtest";

#define LINE "=========================================================="

static void print_banner(void)
{
    esp_chip_info_t ci;
    esp_chip_info(&ci);
    printf("\n%s\n", LINE);
    printf("  ESP32-C6 硬件自检固件\n");
    printf("  编译时间: %s %s\n", __DATE__, __TIME__);
    printf("  芯片: rev %d, %d 核, %sFlash\n", ci.revision, ci.cores,
           (ci.features & CHIP_FEATURE_EMB_FLASH) ? "内置 " : "外挂 ");
    printf("%s\n", LINE);
}

static void run_all(void)
{
    printf("\n[1] I2C 总线扫描 (SDA=GPIO%d, SCL=GPIO%d)\n",
           PIN_I2C_SDA, PIN_I2C_SCL);
    uint8_t addrs[32];
    int n = test_i2c_scan(addrs, sizeof(addrs));
    if (n == 0) {
        printf("    ✗ 未发现任何 I2C 设备 —— 检查上拉电阻与供电\n");
    } else {
        printf("    ✓ 发现 %d 个设备: ", n);
        for (int i = 0; i < n; i++) printf("0x%02X ", addrs[i]);
        printf("\n");
    }

    printf("\n[2] AHT20 温湿度\n");
    float t = 0, h = 0;
    if (test_aht20(&t, &h)) printf("    ✓ %.1f °C / %.1f %%RH\n", t, h);
    else                    printf("    ✗ 读取失败\n");

    printf("\n[3] DS3231 RTC\n");
    int y, mo, d, hh, mm, ss;
    if (test_ds3231(&y, &mo, &d, &hh, &mm, &ss))
        printf("    ✓ %04d-%02d-%02d %02d:%02d:%02d\n", y, mo, d, hh, mm, ss);
    else printf("    ✗ 读取失败\n");

    printf("\n[4] MPU-6500 六轴\n");
    int16_t acc[3] = { 0 }, gyr[3] = { 0 };
    if (test_mpu6500(acc, gyr))
        printf("    ✓ ACC %6d %6d %6d | GYR %6d %6d %6d\n",
               acc[0], acc[1], acc[2], gyr[0], gyr[1], gyr[2]);
    else printf("    ✗ WHO_AM_I 不匹配或通信失败\n");

    printf("\n[5] MMC5603NJ 磁力计\n");
    int32_t mag[3] = { 0 };
    if (test_mmc5603(mag))
        printf("    ✓ RAW %ld %ld %ld (中心值 524288)\n",
               (long)mag[0], (long)mag[1], (long)mag[2]);
    else printf("    ✗ ProductID 不匹配或通信失败\n");

    printf("\n[6] MAX17048 电量计\n");
    float v = 0, soc = 0;
    if (test_max17048(&v, &soc)) printf("    ✓ 电池 %.3f V / %.1f %%\n", v, soc);
    else                         printf("    ✗ 读取失败\n");

    printf("\n[7] ICS-43434 I2S 麦克风\n");
    int32_t peak = 0; int nz = 0;
    if (test_i2s_mic(&peak, &nz))
        printf("    %s 峰值 %ld, 有效样本 %d\n",
               nz > 0 ? "✓" : "△", (long)peak, nz);
    else printf("    ✗ I2S 初始化失败\n");

    printf("\n[8] TF 卡座 (SPI 模式探测)\n");
    uint8_t r1 = 0xFF; uint32_t ocr = 0;
    if (test_sdcard_spi(&r1, &ocr))
        printf("    ✓ CMD0 OK, CMD8 OCR=0x%08lX\n", (unsigned long)ocr);
    else
        printf("    ✗ CMD0 R1=0x%02X (未插卡 / 座子未焊好 / 脚位不符)\n", r1);

    printf("\n[9] 按键 + 蜂鸣器\n");
    test_buttons_init();
    printf("    当前按键状态: BOOT=%d DOWN=%d USER=%d\n",
           (test_buttons_read() >> 0) & 1,
           (test_buttons_read() >> 1) & 1,
           (test_buttons_read() >> 2) & 1);
    test_buzzer_beep(150);

    printf("\n[10] 电子纸 FPC 接口\n");
    bool busy_idle = false;
    if (test_epd_probe(&busy_idle))
        printf("    ✓ 控制线可翻转, BUSY 空闲=%d\n", busy_idle);
    else printf("    ✗ 探测失败\n");

    printf("\n%s\n  自检结束\n%s\n", LINE, LINE);
}

void app_main(void)
{
    print_banner();

    if (test_i2c_bus_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C 总线初始化失败，后续 I2C 测试将全部失败");
    }

    run_all();

    /* 之后进入周期遥测：每 10 秒刷新一次关键读数，按 BOOT 键可重跑全量自检 */
    while (1) {
        float t = 0, h = 0, v = 0, soc = 0;
        bool ok_t = test_aht20(&t, &h);
        bool ok_v = test_max17048(&v, &soc);

        printf("[%llus] ", (unsigned long long)(esp_timer_get_time() / 1000000ULL));
        if (ok_t) printf("温湿度 %.1fC / %.1f%%RH", t, h);
        else      printf("温湿度 --");
        if (ok_v) printf(" | 电池 %.3fV / %.1f%%", v, soc);
        else      printf(" | 电池 --");
        printf("\n");

        if (test_buttons_read() & (1 << 0)) {
            printf("\n>>> BOOT 键触发，重新执行全量自检 <<<\n");
            run_all();
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
