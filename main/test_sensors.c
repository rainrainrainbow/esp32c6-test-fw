#include "test_all.h"
#include "pin_config.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sensor";

/* ---------- 通用寄存器读写助手 ---------- */
static esp_err_t wr8(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t d[2] = { reg, val };
    return i2c_master_write_to_device(I2C_PORT, addr, d, 2, pdMS_TO_TICKS(100));
}

static esp_err_t rd(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_write_read_device(I2C_PORT, addr, &reg, 1, buf, len,
                                        pdMS_TO_TICKS(100));
}

/* ================= AHT20 温湿度 ================= */
bool test_aht20(float *temp_c, float *humi_pct)
{
    uint8_t trig[3] = { 0xAC, 0x33, 0x00 };
    if (i2c_master_write_to_device(I2C_PORT, ADDR_AHT20, trig, 3,
                                   pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGE(TAG, "AHT20: write trigger failed");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(80));

    uint8_t d[7] = { 0 };
    if (i2c_master_read_from_device(I2C_PORT, ADDR_AHT20, d, 7,
                                    pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGE(TAG, "AHT20: read failed");
        return false;
    }
    if ((d[0] & 0x80) != 0) {          /* bit7=1 表示忙 */
        ESP_LOGW(TAG, "AHT20: busy, retry later");
        return false;
    }

    uint32_t hraw = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
    uint32_t traw = ((uint32_t)(d[3] & 0x0F) << 16) |
                    ((uint32_t)d[4] << 8) | d[5];

    *humi_pct = (float)hraw * 100.0f / 1048576.0f;
    *temp_c   = (float)traw * 200.0f / 1048576.0f - 50.0f;
    return true;
}

/* ================= DS3231 RTC ================= */
static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

bool test_ds3231(int *year, int *mon, int *day, int *hour, int *min, int *sec)
{
    uint8_t r[7] = { 0 };
    if (rd(ADDR_DS3231, 0x00, r, 7) != ESP_OK) {
        ESP_LOGE(TAG, "DS3231: read failed");
        return false;
    }
    *sec  = bcd2dec(r[0] & 0x7F);
    *min  = bcd2dec(r[1]);
    *hour = bcd2dec(r[2] & 0x3F);
    *day  = bcd2dec(r[4]);
    *mon  = bcd2dec(r[5] & 0x7F);
    *year = 2000 + bcd2dec(r[6]);
    return true;
}

/* ================= MPU-6500 六轴 ================= */
bool test_mpu6500(int16_t accel[3], int16_t gyro[3])
{
    uint8_t id = 0;
    if (rd(ADDR_MPU6500, 0x75, &id, 1) != ESP_OK || id != 0x70) {
        ESP_LOGE(TAG, "MPU6500: WHO_AM_I=0x%02X (expect 0x70)", id);
        return false;
    }
    wr8(ADDR_MPU6500, 0x6B, 0x00);        /* 退出睡眠 */
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t b[6] = { 0 };
    if (rd(ADDR_MPU6500, 0x3B, b, 6) != ESP_OK) return false;
    accel[0] = (int16_t)((b[0] << 8) | b[1]);
    accel[1] = (int16_t)((b[2] << 8) | b[3]);
    accel[2] = (int16_t)((b[4] << 8) | b[5]);

    if (rd(ADDR_MPU6500, 0x43, b, 6) != ESP_OK) return false;
    gyro[0] = (int16_t)((b[0] << 8) | b[1]);
    gyro[1] = (int16_t)((b[2] << 8) | b[3]);
    gyro[2] = (int16_t)((b[4] << 8) | b[5]);
    return true;
}

/* ================= MMC5603NJ 磁力计 ================= */
bool test_mmc5603(int32_t raw[3])
{
    uint8_t id = 0;
    if (rd(ADDR_MMC5603, 0x39, &id, 1) != ESP_OK || id != 0x10) {
        ESP_LOGE(TAG, "MMC5603: ProductID=0x%02X (expect 0x10)", id);
        return false;
    }
    wr8(ADDR_MMC5603, 0x1B, 0x01);        /* TM_T: 触发单次测量 */
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t b[9] = { 0 };
    if (rd(ADDR_MMC5603, 0x00, b, 9) != ESP_OK) return false;

    /* 20-bit: Xout0<<12 | Xout1<<4 | Xout2>>4，中心值 2^19 */
    raw[0] = ((uint32_t)b[0] << 12) | ((uint32_t)b[1] << 4) | (b[6] >> 4);
    raw[1] = ((uint32_t)b[2] << 12) | ((uint32_t)b[3] << 4) | (b[7] >> 4);
    raw[2] = ((uint32_t)b[4] << 12) | ((uint32_t)b[5] << 4) | (b[8] >> 4);
    return true;
}

/* ================= MAX17048 电量计 ================= */
#define MAX17048_VCELL_LSB_MV  1.25f      /* ⚠️ 按 datasheet 核对后调整 */

bool test_max17048(float *vcell_v, float *soc_pct)
{
    uint8_t b[2] = { 0 };
    if (rd(ADDR_MAX17048, 0x02, b, 2) != ESP_OK) {
        ESP_LOGE(TAG, "MAX17048: read VCELL failed");
        return false;
    }
    uint16_t vraw = (uint16_t)((b[0] << 8) | b[1]);
    *vcell_v = (float)(vraw >> 4) * MAX17048_VCELL_LSB_MV / 1000.0f;

    if (rd(ADDR_MAX17048, 0x04, b, 2) != ESP_OK) {
        ESP_LOGE(TAG, "MAX17048: read SOC failed");
        return false;
    }
    *soc_pct = (float)b[0] + (float)b[1] / 256.0f;
    return true;
}
