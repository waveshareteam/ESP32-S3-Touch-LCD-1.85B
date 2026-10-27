# QMI8658 Test

[English](README.md) | [简体中文](README_ZH.md)

## Function

Read acceleration, angular velocity and the IMU internal temperature through the BSP.

## Hardware

ESP32-S3-Touch-LCD-1.85B and a USB cable.

## Run

Use ESP-IDF v5.5.4 from this example directory:

```sh
idf.py set-target esp32s3 build flash monitor
```

## Expected behavior

The serial monitor updates acceleration in g and angular velocity in degrees/s every 500 ms.

## FAQ

The reported temperature is the IMU die temperature, not ambient temperature.
