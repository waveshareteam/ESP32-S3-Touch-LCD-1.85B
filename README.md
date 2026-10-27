<div align="center">
  <h1>ESP32-S3-Touch-LCD-1.85B</h1>
  <p><strong>360 × 360 round touch display with dual microphones, audio, motion sensor, RTC and battery gauge</strong></p>
  <p><strong>English</strong> | <a href="README_ZH.md">简体中文</a></p>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B/actions/workflows/build.yml"><img alt="Build" src="https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B/actions/workflows/build.yml/badge.svg"></a>
    <a href="https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B/releases/latest"><img alt="Latest Release" src="https://img.shields.io/github/v/release/waveshareteam/ESP32-S3-Touch-LCD-1.85B"></a>
    <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/waveshareteam/ESP32-S3-Touch-LCD-1.85B"></a>
  </p>
  <p>
    <a href="https://www.waveshare.com/esp32-s3-touch-lcd-1.85b.htm">🌐 Product</a> &middot;
    <a href="Firmware/">📦 Firmware</a> &middot;
    <a href="examples/esp-idf/">🧩 ESP-IDF</a> &middot;
    <a href="examples/Arduino-V3.2.0/">🔧 Arduino</a> &middot;
    <a href="hardware/">📚 Documentation</a>
  </p>
  <p><img src="docs/images/esp32-s3-touch-lcd-1.85b.jpg" alt="ESP32-S3-Touch-LCD-1.85B development board" width="500"></p>
</div>

---

## ✨ Overview

ESP32-S3-Touch-LCD-1.85B combines a round touch display with audio, a motion sensor, RTC, battery gauge and microSD interface. This repository provides standalone peripheral examples and the complete factory application source.

| Feature | Device / configuration |
| --- | --- |
| MCU | ESP32-S3 |
| Memory | 16 MB Flash, 8 MB PSRAM |
| Display and touch | 1.85-inch, 360 × 360 round touch display |
| Audio | ES8311, ES7210, dual microphones |
| Motion and clock | QMI8658, PCF85063 |
| Battery gauge | BQ27220, 500 mAh battery profile |

## 📦 Quick Start

Install ESP-IDF v5.5.4, open the required example directory and run:

```sh
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

Each example README describes setup, operation and expected results. Existing factory binaries are in [Firmware](Firmware/); see the [factory source guide](examples/factory/README.md) for the complete application.

## 🧪 Examples

### ESP-IDF

| Example | Main feature |
| --- | --- |
| [01_lvgl_demo](examples/esp-idf/01_lvgl_demo/) | Round-screen LVGL, touch and backlight control |
| [02_sd_card](examples/esp-idf/02_sd_card/) | Four-bit SDMMC, file read/write verification and on-screen results |
| [03_qmi8658](examples/esp-idf/03_qmi8658/) | Acceleration, angular velocity and die temperature |
| [04_pcf85063](examples/esp-idf/04_pcf85063/) | RTC calendar reading and setting |
| [05_bq27220](examples/esp-idf/05_bq27220/) | Battery charge, voltage, current and capacity |
| [06_mic_test](examples/esp-idf/06_mic_test/) | Dual microphone meters, recording and per-channel playback |
| [factory](examples/factory/) | Complete desktop, settings, media and voice chat applications |

All six basic examples and the factory project use the same local BSP implementation. Each basic example initializes only its required peripherals.

### Arduino

Original sketches and bundled libraries are in [examples/Arduino-V3.2.0](examples/Arduino-V3.2.0/), with their existing usage instructions.

## 🛠️ Development Environment

| Framework / library | Version | Scope |
| --- | --- | --- |
| ESP-IDF | v5.5.4 | Six basic examples and factory project |
| LVGL | 9.5.0 | ESP-IDF display examples and factory project |
| Arduino-ESP32 | 3.2.0 | Original Arduino examples |

## 📚 Resources

- [Hardware documentation](hardware/)
- [Product page](https://www.waveshare.com/esp32-s3-touch-lcd-1.85b.htm)
- [Product Wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.85B)

## 🤝 Support

- Example or repository problems: [open an issue](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B/issues).
- Product usage and hardware questions: [Waveshare support](https://service.waveshare.com/).

## 📄 License

Apache License 2.0. See [LICENSE](LICENSE). Third-party components retain the licenses in their own directories.
