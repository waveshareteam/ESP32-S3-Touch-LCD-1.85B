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
    <a href="firmware/">📦 Firmware</a> &middot;
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

1. Download `ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20261027.bin` from [factory firmware](firmware/README.md).
2. Install the flashing tool: `python -m pip install esptool`.
3. Connect the board with a USB data cable and identify its serial port.
4. Run these commands in the firmware directory, replacing `<PORT>` with the board port. Erasing clears settings stored on the board; back up required data first.

   ```sh
   python -m esptool --chip esp32s3 --port <PORT> erase_flash
   python -m esptool --chip esp32s3 --port <PORT> write_flash 0x0 ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20261027.bin
   ```

5. Reset the board, connect Wi-Fi in Settings and open an application from the desktop.

Factory images are flashed at `0x0`; see the [firmware guide](firmware/README.md) to select a version. To build from source, follow the individual example guides or the [factory application guide](examples/factory/README.md).

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

Use the standalone examples to test the display, touch, storage, sensors, clock, battery and dual microphone audio. The factory application provides the complete desktop and combined feature demonstrations.

### Arduino

| Example | Main feature |
| --- | --- |
| [01_lvgl_demo](examples/Arduino-V3.2.0/examples/01_lvgl_demo/) | LVGL display and touch |
| [02_lvgl_BQ27220](examples/Arduino-V3.2.0/examples/02_lvgl_BQ27220/) | Battery gauge display |
| [03_audio_out_no_tf](examples/Arduino-V3.2.0/examples/03_audio_out_no_tf/) | Audio output without an SD card |
| [04_SDMMC_Test](examples/Arduino-V3.2.0/examples/04_SDMMC_Test/) | SDMMC card test |
| [05_audio_out_tf](examples/Arduino-V3.2.0/examples/05_audio_out_tf/) | Audio playback from an SD card |
| [06_esp_sr](examples/Arduino-V3.2.0/examples/06_esp_sr/) | Speech recognition |
| [07_I2C_qmi8658](examples/Arduino-V3.2.0/examples/07_I2C_qmi8658/) | QMI8658 motion sensor |
| [08_I2C_pcf85063](examples/Arduino-V3.2.0/examples/08_I2C_pcf85063/) | PCF85063 RTC |

Use the matching libraries provided in the [bundled libraries directory](examples/Arduino-V3.2.0/libraries/).

## 🛠️ Development Environment

| Framework / library | Version | Scope |
| --- | --- | --- |
| ESP-IDF | v5.5.4 | Six basic examples and factory project |
| LVGL | 9.5.0 | ESP-IDF display examples and factory project |
| Arduino-ESP32 | 3.2.0 | Eight Arduino examples |

## 📚 Resources

- [Schematic Rev1.1](hardware/ESP32-S3-Touch-LCD-1.85B%20Rev1.1.pdf)
- [Product page](https://www.waveshare.com/esp32-s3-touch-lcd-1.85b.htm)
- [Product Wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.85B)

## 🤝 Support

- Example or repository problems: [open an issue](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B/issues).
- Product usage and hardware questions: [Waveshare support](https://service.waveshare.com/).

## 📄 License

Apache License 2.0. See [LICENSE](LICENSE). Third-party components retain the licenses in their own directories.
