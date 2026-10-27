# SD Card Read/Write and Display

[English](README.md) | [简体中文](README_ZH.md)

## Function

Verify microSD text-file write/read and show the result and file contents on the 360 × 360 round display. The serial monitor also reports card capacity and free space.

## Hardware

ESP32-S3-Touch-LCD-1.85B, a USB cable and a FAT32 microSD card. Insert the card before power-on; each test overwrites only the root file `test.txt` and never formats the card.

## Run

Use ESP-IDF v5.5.4 and run the following in this example directory; the test starts automatically after flashing:

```sh
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## Expected behavior

The screen shows **Write / read passed**, `ESP_OK`, `/sdcard/test.txt` and the text `ESP32-S3-Touch-LCD-1.85B SD test`. The card is unmounted after the test; press RESET to run it again.

## FAQ

Mount, write, read or unmount failures remain visible on the screen with a retry hint; check the serial log for details. Insert a writable FAT32 card and press RESET to retry; inserting a card alone does not repeat the test.
