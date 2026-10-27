# BQ27220 Battery Test

[English](README.md) | [简体中文](README_ZH.md)

## Function

Use the factory 500 mAh profile and read battery percentage, voltage, current, capacities and gauge status.

## Hardware

ESP32-S3-Touch-LCD-1.85B and a USB cable. Connect the supplied 3.7 V, 500 mAh battery.

## Run

Use ESP-IDF v5.5.4 from this example directory:

```sh
idf.py set-target esp32s3 build flash monitor
```

## Expected behavior

The serial monitor updates once per second. Positive current indicates charging and negative current indicates discharging.

## FAQ

Connect the supplied battery before USB power. BIN is pulled down on this board, so USB-only or hot-plug readings do not reliably identify battery presence or charge level; temperature is the gauge internal temperature.
