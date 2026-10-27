# PCF85063 RTC Test

[English](README.md) | [简体中文](README_ZH.md)

## Function

Read the hardware RTC calendar independently of network time; retain a valid calendar across resets.

## Hardware

ESP32-S3-Touch-LCD-1.85B and a USB cable.

## Run

Use ESP-IDF v5.5.4 from this example directory:

```sh
idf.py set-target esp32s3 build flash monitor
```

## Expected behavior

The serial monitor prints the calendar once per second. An invalid or stopped calendar is initialized to 2025-01-01 12:00:00.

## FAQ

Set SET_TIME_ON_BOOT and the initial calendar in main/main.cpp to demonstrate setting the time. RTC retention requires its supply to remain powered.
