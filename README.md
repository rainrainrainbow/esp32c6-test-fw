# ESP32-C6 硬件自检固件 + GitHub Actions CI

根据提供的原理图 Netlist 还原出的板载资源，编写的一套上电自检固件：
上电后依次探测 I2C 总线及 5 个从设备、I2S 麦克风、TF 卡座、按键、蜂鸣器、电子纸接口，
随后进入 10 秒周期的遥测循环；按下 BOOT 键可重跑全量自检。

## 1. 板载资源与引脚映射

| 功能 | 器件 | 接口 | 引脚 |
|---|---|---|---|
| 主控 | ESP32-C6-MINI-1-N4 | — | — |
| 温湿度 | AHT20 | I2C 0x38 | SDA=GPIO6 / SCL=GPIO7 |
| RTC | DS3231MZ/V+ | I2C 0x68，INT→GPIO15 | 同上 |
| 六轴 | MPU-6500 | I2C 0x69（AD0 接 VCC），INT→GPIO19 | 同上 |
| 磁力计 | MMC5603NJ | I2C 0x30 | 同上 |
| 电量计 | MAX17048G+T10 | I2C 0x36 | 同上 |
| 麦克风 | ICS-43434 | I2S | BCLK=GPIO16 / WS=GPIO17 / DIN=GPIO23 |
| TF 卡座 | 自弹 TF 座 TFAA | SPI 模式 | CLK=GPIO21 / CMD=GPIO20 / DAT0=GPIO22 / CS=GPIO8 |
| 电子纸 | FPC-0.5MM-24P | 软件 SPI | CS=GPIO14 / RST=GPIO0 / BUSY=GPIO1 / SCLK=GPIO4 / MOSI=GPIO5 |
| 按键 | SW-SMD ×3 | GPIO 输入 | BOOT=GPIO9 / DOWN=GPIO3 / USER=GPIO2 |
| 蜂鸣器 | — | LEDC PWM | GPIO18 |
| 音频功放 | NS4150B | — | CTRL 经 10k 上拉到 VCC（常开），输入来源 Netlist 中未明确 |
| 充电 | LTC4054 | — | PROG 接 4.7k → 约 250mA 充电电流 |
| USB | Type-C | USB Serial/JTAG | D-=GPIO12 / D+=GPIO13 |

> I2S 的 GPIO16/GPIO17 是依据模组第 31/30 脚标注的 `TXD0/RXD0` 推测的，
> 若与实际不符，只改 `main/pin_config.h` 一处即可。

## 2. 本地编译与烧录

需要 ESP-IDF **v5.2 及以上**（CI 固定用 v5.4）。
`main/CMakeLists.txt` 里用的是 v5.2+ 拆分后的驱动组件名（`esp_driver_i2c` / `esp_driver_i2s` /
`esp_driver_spi` / `esp_driver_ledc` / `esp_driver_gpio`）；若您用更早的 IDF，
把这几个名字统一替换成 `driver` 即可。

```bash
get_idf                              # 或 source $IDF_PATH/export.sh
cd esp32c6-test-fw
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyACM0 flash monitor # C6 的 USB Serial/JTAG 通常是 ACM 口
```

串口波特率无需设置（USB Serial/JTAG 固定）。查看日志用 `idf.py monitor` 即可。

## 3. GitHub Actions

工作流在 `.github/workflows/build.yml`，包含三个 job：

1. **build** — 在 `espressif/idf:release-v5.4` 容器里 `set-target esp32c6` → `build` → `size`，
   并上传 `bootloader.bin` / `partition-table.bin` / 应用 bin 到 Artifacts。
2. **secret-scan** — 用 gitleaks 全量历史扫描，防止 Token、私钥被误提交。
3. **release** — 仅当推送 `v*` tag 时触发，把 bin 打成 GitHub Release。

### 关于凭证（重要）

这套 CI **不需要任何个人访问令牌（PAT）**：
`secrets.GITHUB_TOKEN` 由 GitHub 在每次运行时自动注入，权限由 `permissions:` 显式限定为最小集。

如果确实要接入外部服务（如私有镜像仓库），请把凭证存到
**Settings → Secrets and variables → Actions**，在 workflow 里用 `${{ secrets.XXX }}` 引用；
**绝不要把明文 Token 写进代码、workflow 或提交历史**。
> 您此前在对话中贴出的 `ghp_...` 令牌请立即 Revoke / Regenerate —— 明文出现即视为已泄露。

## 4. 上电自检输出示例

```
==========================================================
  ESP32-C6 硬件自检固件
  芯片: rev 0, 1 核, 内置 Flash
==========================================================

[1] I2C 总线扫描 (SDA=GPIO6, SCL=GPIO7)
    ✓ 发现 5 个设备: 0x30 0x36 0x38 0x68 0x69
[2] AHT20 温湿度
    ✓ 26.3 °C / 48.7 %RH
...
[7] ICS-43434 I2S 麦克风
    ✓ 峰值 18432, 有效样本 511
[8] TF 卡座 (SPI 模式探测)
    ✓ CMD0 OK, CMD8 OCR=0x000001AA
```

## 5. 从 Netlist 中发现的几个设计风险（建议同步核对）

1. **I2C 缺少外部上拉**：GPIO6/GPIO7 网络上只看到从设备，没有 4.7kΩ 上拉。
   固件已开启内部 ~45kΩ 上拉兜底，但 5 个从设备总线电容较大，建议硬件补 4.7kΩ 到 3.3V。
2. **TF 卡按 SD 模式命名但实际只能跑 SPI**：ESP32-C6 不带 SDMMC 主机控制器，
   Netlist 里的 CMD/CLK/DAT0 命名是 SD 模式。固件已改走 SD 卡的 SPI 模式复用同一组脚位。
3. **NS4150B 功放输入来源不明**：Netlist 中 `NS_PA_INP/INN` 只连到 RC/阻容网络，
   未看到来自 ESP32 的 DAC/I2S 输出脚；疑似由 GPIO18（BUZZ）经滤波网络送入，请确认。
4. **D 类功放输出缺 LC 滤波**：`VoP/VON` 直连喇叭端子，建议串磁珠 + 1nF 对地，抑制 EMI。
5. **Strapping 脚占用**：GPIO9（BOOT 键）、GPIO0（EPD_RST）、GPIO2（USER 键）可能与
   ESP32-C6 的启动配置脚重叠，请核对 datasheet 确认上电时的默认电平不会误触发下载模式。
