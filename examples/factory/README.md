# Factory Application

[English](README.md) | [简体中文](README_ZH.md)

## Function

A 360 × 360 round-screen desktop with four icons per page: Display Test, Settings, AIChats, Battery, Touch Test, Photos, Weather, Music, Sensor, 2048, Mic Test, Button Test, Video and Gravity. Swipe left or right to change pages; voice chat opens directly in AIChats.

## Hardware

ESP32-S3-Touch-LCD-1.85B, a speaker and the supplied 3.7 V, 500 mAh battery; online features need 2.4 GHz Wi-Fi. For media, insert a FAT32 microSD card with JPEG images in `photo/`, MP3 files in `music/`, and AVI/MJPEG videos in `video/` (frames up to 360 × 360; video audio tracks are not played).

## Run

For a ready-to-flash image, follow the [firmware guide](../../firmware/README.md). To build this application, use ESP-IDF v5.5.4 and run:

```sh
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

Connect Wi-Fi in Settings, or connect to the `S3-LCD-1.85B-xxxxxx` hotspot with password `12345678` and open `http://192.168.4.1`. Open AIChats after connecting and follow its activation instructions if prompted.

## Expected behavior

The desktop shows time, Wi-Fi and battery status; Photos fills the round screen, and Video displays a 240 × 240 preview. Mic Test shows separate MIC1/MIC2 levels and records up to five seconds for per-channel playback; AIChats supports voice conversation, wake-word interruption, and volume and brightness commands.

## FAQ

The Sensor RTC calendar runs independently; network time updates the desktop clock. Connect the battery before USB for battery measurements: USB-only or hot-plug readings can be inaccurate, and gauge temperature is not ambient temperature; recordings are temporary, and you should exit the current audio application before opening another.
