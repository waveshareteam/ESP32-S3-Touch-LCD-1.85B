<div align="center">
  <h1>ESP32-S3-Touch-LCD-1.85B</h1>
  <p><strong>360 × 360 圆形触摸屏开发板，配备双麦克风、音频、运动传感器、RTC 和电池电量计</strong></p>
  <p><a href="README.md">English</a> | <strong>简体中文</strong></p>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B/actions/workflows/build.yml"><img alt="Build" src="https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B/actions/workflows/build.yml/badge.svg"></a>
    <a href="https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B/releases/latest"><img alt="Latest Release" src="https://img.shields.io/github/v/release/waveshareteam/ESP32-S3-Touch-LCD-1.85B"></a>
    <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/waveshareteam/ESP32-S3-Touch-LCD-1.85B"></a>
  </p>
  <p>
    <a href="https://www.waveshare.net/shop/ESP32-S3-Touch-LCD-1.85B.htm">🌐 产品</a> &middot;
    <a href="firmware/">📦 出厂固件</a> &middot;
    <a href="examples/esp-idf/">🧩 ESP-IDF</a> &middot;
    <a href="examples/Arduino-V3.2.0/">🔧 Arduino</a> &middot;
    <a href="hardware/">📚 硬件资料</a>
  </p>
  <p><img src="docs/images/esp32-s3-touch-lcd-1.85b.jpg" alt="ESP32-S3-Touch-LCD-1.85B development board" width="500"></p>
</div>

---

## ✨ 产品概述

ESP32-S3-Touch-LCD-1.85B 集成圆形触摸显示屏及音频、运动传感器、RTC、电池电量计和 microSD 接口。本仓库提供独立外设示例和完整出厂应用源码。

| 功能 | 器件 / 配置 |
| --- | --- |
| 主控 | ESP32-S3 |
| 存储 | 16 MB Flash、8 MB PSRAM |
| 显示与触摸 | 1.85 英寸，360 × 360 圆形触摸屏 |
| 音频 | ES8311、ES7210，双麦克风 |
| 传感器与时钟 | QMI8658、PCF85063 |
| 电池电量计 | BQ27220，500 mAh 电池配置 |

## 📦 快速开始

1. 从[出厂固件目录](firmware/README_ZH.md)下载最新版 `ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20261027.bin`。
2. 安装烧录工具：`python -m pip install esptool`。
3. 使用 USB 数据线连接开发板，确认其串口名称。
4. 在固件所在目录执行以下命令，将 `<PORT>` 替换为开发板串口；擦除会清除板上保存的设置，请先备份所需数据。

   ```sh
   python -m esptool --chip esp32s3 --port <PORT> erase_flash
   python -m esptool --chip esp32s3 --port <PORT> write_flash 0x0 ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20261027.bin
   ```

5. 复位后进入桌面，在 Settings 中连接 Wi-Fi，再打开所需应用。

出厂固件从 `0x0` 烧录；版本选择见[固件说明](firmware/README_ZH.md)。自行编译请参阅各示例说明或[出厂应用说明](examples/factory/README_ZH.md)。

## 🧪 示例

### ESP-IDF

| 示例 | 功能 |
| --- | --- |
| [01_lvgl_demo](examples/esp-idf/01_lvgl_demo/README_ZH.md) | 圆形屏 LVGL、触摸与背光调节 |
| [02_sd_card](examples/esp-idf/02_sd_card/README_ZH.md) | 四线 SDMMC、文件读写校验及屏幕结果显示 |
| [03_qmi8658](examples/esp-idf/03_qmi8658/README_ZH.md) | 加速度、角速度及芯片温度 |
| [04_pcf85063](examples/esp-idf/04_pcf85063/README_ZH.md) | RTC 日历读取与设置 |
| [05_bq27220](examples/esp-idf/05_bq27220/README_ZH.md) | 电量、电压、电流及容量读取 |
| [06_mic_test](examples/esp-idf/06_mic_test/README_ZH.md) | 双麦克风音量条、录音及分通道回放 |
| [factory](examples/factory/README_ZH.md) | 完整桌面、设置、媒体应用和语音对话 |

基础示例可分别测试显示、触摸、SD 卡、传感器、时钟、电池和双麦音频；出厂应用提供完整桌面及综合功能演示。

### Arduino

| 示例 | 功能 |
| --- | --- |
| [01_lvgl_demo](examples/Arduino-V3.2.0/examples/01_lvgl_demo/) | LVGL 显示与触摸 |
| [02_lvgl_BQ27220](examples/Arduino-V3.2.0/examples/02_lvgl_BQ27220/) | 电池电量计显示 |
| [03_audio_out_no_tf](examples/Arduino-V3.2.0/examples/03_audio_out_no_tf/) | 无需 SD 卡的音频输出 |
| [04_SDMMC_Test](examples/Arduino-V3.2.0/examples/04_SDMMC_Test/) | SDMMC 存储卡测试 |
| [05_audio_out_tf](examples/Arduino-V3.2.0/examples/05_audio_out_tf/) | SD 卡音频播放 |
| [06_esp_sr](examples/Arduino-V3.2.0/examples/06_esp_sr/) | 语音识别 |
| [07_I2C_qmi8658](examples/Arduino-V3.2.0/examples/07_I2C_qmi8658/) | QMI8658 运动传感器 |
| [08_I2C_pcf85063](examples/Arduino-V3.2.0/examples/08_I2C_pcf85063/) | PCF85063 RTC |

所需库位于[随附 libraries 目录](examples/Arduino-V3.2.0/libraries/)，请使用对应版本编译。

## 🛠️ 开发环境

| 框架 / 库 | 版本 | 适用范围 |
| --- | --- | --- |
| ESP-IDF | v5.5.4 | 6 个基础示例及出厂工程 |
| LVGL | 9.5.0 | ESP-IDF 显示示例及出厂工程 |
| Arduino-ESP32 | 3.2.0 | 8 个 Arduino 示例 |

## 📚 资源

- [原理图 Rev1.1](hardware/ESP32-S3-Touch-LCD-1.85B%20Rev1.1.pdf)
- [产品页面](https://www.waveshare.net/shop/ESP32-S3-Touch-LCD-1.85B.htm)
- [产品 Wiki](https://docs.waveshare.net/ESP32-S3-Touch-LCD-1.85B/)

## 🤝 支持

产品使用、示例运行或硬件问题，请联系[微雪技术支持](https://docs.waveshare.net/ESP32-S3-Touch-LCD-1.85B/Technical-Support/)。

## 📄 许可证

仓库采用 Apache License 2.0，见 [LICENSE](LICENSE)。第三方组件遵循各自目录中的许可证。
