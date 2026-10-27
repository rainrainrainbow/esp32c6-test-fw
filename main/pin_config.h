/*
 * pin_config.h —— 引脚映射（依据提供的 Netlist 还原）
 *
 * ⚠️ 重要：以下 GPIO 编号是依据 Netlist 中 "IOxx" 的网络名直接映射得到的。
 *    其中模组第 30/31 脚在 Netlist 中标注为 RXD0 / TXD0（而非 IOxx），
 *    这里按 ESP32-C6-MINI-1-N4 的 UART0 默认脚推测为 GPIO17 / GPIO16。
 *    烧录前请用万用表 / 模组 datasheet 核对，若不符只改本文件即可。
 */

#pragma once
#include "driver/gpio.h"
#include "driver/i2c.h"

/* ---------------- I2C 总线（SDA=GPIO6, SCL=GPIO7） ----------------
 * Netlist 显示 GPIO6/GPIO7 上挂了 5 个从设备，但网络上没有看到外部上拉电阻。
 * 固件里已启用 ESP32-C6 内部上拉（约 45kΩ）做兜底；
 * 若在 400kHz 或长线通信下不稳定，硬件上仍需补 4.7kΩ 外部上拉到 3V3。
 */
#define I2C_PORT          I2C_NUM_0
#define PIN_I2C_SDA       GPIO_NUM_6
#define PIN_I2C_SCL       GPIO_NUM_7
#define I2C_FREQ_HZ       100000

/* I2C 从机地址（7-bit） */
#define ADDR_AHT20        0x38
#define ADDR_DS3231       0x68
#define ADDR_MPU6500      0x69   /* SDO/AD0 接 VCC -> 地址 0x69 */
#define ADDR_MMC5603      0x30
#define ADDR_MAX17048     0x36

/* ---------------- I2S 数字麦克风 ICS-43434 ----------------
 * Netlist: IIS_SCK -> ESP32 TXD0(31), IIS_WS -> RXD0(30), SD -> IO23
 */
#define PIN_I2S_BCLK      GPIO_NUM_16   /* 模组 31 脚 TXD0 —— 需核对 */
#define PIN_I2S_WS        GPIO_NUM_17   /* 模组 30 脚 RXD0 —— 需核对 */
#define PIN_I2S_DIN       GPIO_NUM_23
#define I2S_MIC_RATE      16000

/* ---------------- TF 卡座（SPI 模式） ----------------
 * 原理图标为 CMD/CLK/DAT0/CD-DAT3（SD 模式命名）。
 * ESP32-C6 不带 SDMMC 主机控制器，这里改用 SD 卡的 SPI 模式复用同一组脚：
 *   CLK->SCLK, CMD->MOSI, DAT0->MISO, CD/DAT3->CS
 */
#define PIN_SD_CLK        GPIO_NUM_21
#define PIN_SD_MOSI       GPIO_NUM_20
#define PIN_SD_MISO       GPIO_NUM_22
#define PIN_SD_CS         GPIO_NUM_8
#define SD_SPI_HOST       SPI2_HOST

/* ---------------- 电子纸 FPC-24P ----------------
 * Netlist 中 FPC 侧没有看到硬件 SPI 的 MOSI/SCLK，仅 GPIO4 / GPIO5 两根，
 * 因此测试固件按“软件模拟 SPI”处理：SCLK=GPIO4, MOSI=GPIO5。
 * 真实屏的初始化序列需拿到屏幕 IC datasheet 后替换。
 */
#define PIN_EPD_CS        GPIO_NUM_14
#define PIN_EPD_RST       GPIO_NUM_0
#define PIN_EPD_BUSY      GPIO_NUM_1
#define PIN_EPD_SCLK      GPIO_NUM_4
#define PIN_EPD_MOSI      GPIO_NUM_5

/* ---------------- 按键（低电平有效，外部已上拉到 VCC） ---------------- */
#define PIN_KEY_BOOT      GPIO_NUM_9    /* BOOT_KEY —— 注意是 strapping 脚 */
#define PIN_KEY_DOWN      GPIO_NUM_3
#define PIN_KEY_USER      GPIO_NUM_2

/* ---------------- 蜂鸣器 / 音频功放驱动 ---------------- */
#define PIN_BUZZER        GPIO_NUM_18
