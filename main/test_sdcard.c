#include "test_all.h"
#include "pin_config.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sdcard";

static spi_device_handle_t s_dev = NULL;

static uint8_t spi_xfer(uint8_t tx)
{
    uint8_t rx = 0xFF;
    spi_transaction_t t = {
        .length    = 8,
        .tx_buffer = &tx,
        .rx_buffer = &rx,
    };
    spi_device_transmit(s_dev, &t);
    return rx;
}

static uint8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t p[6] = { (uint8_t)(0x40 | cmd),
                     (uint8_t)(arg >> 24), (uint8_t)(arg >> 16),
                     (uint8_t)(arg >> 8),  (uint8_t)arg, crc };
    spi_xfer(0xFF);                       /* 一个字节的前导 */
    for (int i = 0; i < 6; i++) spi_xfer(p[i]);

    for (int i = 0; i < 16; i++) {        /* 等待 R1 响应 */
        uint8_t r = spi_xfer(0xFF);
        if ((r & 0x80) == 0) return r;
    }
    return 0xFF;
}

bool test_sdcard_spi(uint8_t *r1, uint32_t *ocr)
{
    spi_bus_config_t bus = {
        .mosi_io_num     = PIN_SD_MOSI,
        .miso_io_num     = PIN_SD_MISO,
        .sclk_io_num     = PIN_SD_CLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 512,
    };
    if (spi_bus_initialize(SD_SPI_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        ESP_LOGE(TAG, "spi bus init failed");
        return false;
    }
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 400 * 1000,     /* 初始化阶段必须 ≤400kHz */
        .mode           = 0,
        .spics_io_num   = -1,             /* CS 手动控制 */
        .queue_size     = 1,
    };
    if (spi_bus_add_device(SD_SPI_HOST, &devcfg, &s_dev) != ESP_OK) {
        ESP_LOGE(TAG, "spi add device failed");
        return false;
    }

    gpio_set_direction(PIN_SD_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_SD_CS, 1);

    /* ≥74 个 dummy clock */
    for (int i = 0; i < 16; i++) spi_xfer(0xFF);
    vTaskDelay(pdMS_TO_TICKS(2));

    gpio_set_level(PIN_SD_CS, 0);
    uint8_t resp = 0xFF;
    for (int i = 0; i < 10; i++) {
        resp = sd_cmd(0, 0x00000000, 0x95);   /* CMD0: GO_IDLE_STATE */
        if (resp == 0x01) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    *r1 = resp;

    *ocr = 0;
    if (resp == 0x01) {
        uint8_t r7 = sd_cmd(8, 0x000001AA, 0x87);   /* CMD8: SEND_IF_COND */
        uint32_t o = 0;
        for (int i = 0; i < 4; i++) o = (o << 8) | spi_xfer(0xFF);
        *ocr = o;
        ESP_LOGI(TAG, "CMD8 R1=0x%02X OCR=0x%08lX", r7, (unsigned long)o);
    }
    gpio_set_level(PIN_SD_CS, 1);

    spi_bus_remove_device(s_dev);
    spi_bus_free(SD_SPI_HOST);

    if (resp != 0x01) {
        ESP_LOGW(TAG, "CMD0 未返回 0x01 (得到 0x%02X) —— 检查卡是否插入 / 座子焊接", resp);
        return false;
    }
    return true;
}
