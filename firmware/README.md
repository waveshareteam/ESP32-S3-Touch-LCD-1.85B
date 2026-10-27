# Factory Firmware

[English](README.md) | [简体中文](README_ZH.md)

Download a merged image for ESP32-S3-Touch-LCD-1.85B and flash it at `0x0`. Choose the latest version for the current desktop and peripheral applications.

| File | Purpose |
| --- | --- |
| [ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20261027.bin](ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20261027.bin) | Current factory application: round desktop, settings, media, peripheral tests and voice chat |
| [ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20260328.bin](ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20260328.bin) | Earlier factory demonstration |

## Flash

Install esptool with `python -m pip install esptool`, connect the board over USB, and replace `<PORT>` with its serial port. Erasing clears saved settings; back up required data first.

```sh
python -m esptool --chip esp32s3 --port <PORT> erase_flash
python -m esptool --chip esp32s3 --port <PORT> write_flash 0x0 ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20261027.bin
```

Reset the board after flashing. The current application source and usage instructions are in [examples/factory](../examples/factory/README.md); the earlier binary has a different application set.
