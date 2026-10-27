# Board support

[English](README.md) | [简体中文](README_ZH.md)

## Function

Support the 360 x 360 ST77916 round LCD, CST816 touch, ES8311/ES7210 audio, PCF85063 RTC, QMI8658, BQ27220, BOOT and four-bit SDMMC.

## Hardware

ESP32-S3-Touch-LCD-1.85B. Pins are defined in `include/bsp/esp32_s3_touch_lcd_1_85B.h`.

## Run

Include `bsp/esp-bsp.h` and initialize only the peripherals required by the example. Use `bsp_init()` for complete factory startup and `bsp_get_init_status()` for optional peripheral results.

## Expected behavior

`bsp_display_show()` enables the backlight after the first frame; display access uses the BSP lock. RTC and battery data come from the board peripherals; audio session ownership and PCM adaptation use `bsp_extra`.

## FAQ

Initialize hardware before starting worker tasks, acquire an SD lease during file I/O, and release audio sessions after stopping I/O. Peripheral handles belong to the BSP.
