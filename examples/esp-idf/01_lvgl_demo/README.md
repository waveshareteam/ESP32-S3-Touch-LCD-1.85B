# LVGL Touch Demo

[English](README.md) | [简体中文](README_ZH.md)

## Function

Show a simple circular-screen layout with a touch counter and backlight slider.

## Hardware

ESP32-S3-Touch-LCD-1.85B and a USB cable.

## Run

Use ESP-IDF v5.5.4 from this example directory:

```sh
idf.py set-target esp32s3 build flash monitor
```

## Expected behavior

The counter increases when the button is tapped; the slider changes screen brightness.

## FAQ

None.
