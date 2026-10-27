# SD Card Test

[English](README.md) | [简体中文](README_ZH.md)

## Function

Mount the four-bit SDMMC card, report capacity and free space, and verify file write/read.

## Hardware

ESP32-S3-Touch-LCD-1.85B and a USB cable. A FAT32 microSD card is required.

## Run

Use ESP-IDF v5.5.4 from this example directory:

```sh
idf.py set-target esp32s3 build flash monitor
```

## Expected behavior

The serial monitor reports the card information and write/read PASS; the card is then unmounted.

## FAQ

Insert a FAT32 microSD card before reset. The test overwrites only /bsp_sd_test.txt and does not format the card.
