#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/* ---- I2C 总线 ---- */
esp_err_t test_i2c_bus_init(void);
int       test_i2c_scan(uint8_t *addrs, size_t max);

/* ---- I2C 上的传感器 ---- */
bool test_aht20(float *temp_c, float *humi_pct);
bool test_ds3231(int *year, int *mon, int *day, int *hour, int *min, int *sec);
bool test_mpu6500(int16_t accel[3], int16_t gyro[3]);
bool test_mmc5603(int32_t raw[3]);
bool test_max17048(float *vcell_v, float *soc_pct);

/* ---- 数字麦克风 ---- */
bool test_i2s_mic(int32_t *peak, int *nonzero_samples);

/* ---- TF 卡（SPI 模式探测） ---- */
bool test_sdcard_spi(uint8_t *r1, uint32_t *ocr);

/* ---- 按键 / 蜂鸣器 ---- */
void test_buttons_init(void);
uint8_t test_buttons_read(void);
void test_buzzer_beep(int ms);

/* ---- 电子纸接口探测 ---- */
bool test_epd_probe(bool *busy_idle);
