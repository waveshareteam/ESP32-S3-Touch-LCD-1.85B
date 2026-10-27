# ESP32-S3-Touch-LCD-1.85B Factory Firmware

[English](README.md) | [简体中文](README_ZH.md)

## Function

A complete 360 x 360 round-screen desktop with four applications per page; the first page contains Display Test, Settings, AIChats and Battery. Other applications include Touch Test, Photos, Weather, Music, Sensor, 2048, Mic Test, Button Test, VideoPlayer and the original gravity game; voice chat runs inside AIChats without switching firmware partitions.

## Hardware

ESP32-S3-Touch-LCD-1.85B with 16 MB Flash and 8 MB PSRAM, a speaker and the supplied 3.7 V, 500 mAh battery; online functions require 2.4 GHz Wi-Fi. Media applications require a FAT-formatted microSD card with JPEG images in photo/, MP3 files in music/, and AVI/MJPEG videos in video/ (frames up to 360 x 360; AVI audio tracks are not played).

## Run

Use ESP-IDF v5.5.4 from this directory:

```sh
idf.py build flash monitor
```

Configure Wi-Fi in Settings, or connect to the S3-LCD-1.85B-xxxxxx hotspot (password 12345678) and open http://192.168.4.1; without saved credentials the station initially tries weather-wifi / 12345678.

## Expected behavior

The backlight turns on after the first frame, and horizontal swipes change desktop pages; Photos fills the round screen with a centered crop, while video uses a 240 x 240 preview area. Mic Test displays both microphone levels and records up to five seconds in PSRAM for separate MIC1/MIC2 playback; AIChats uses both microphones and the MIC3 playback reference for echo cancellation, wake-word interruption, and voice commands for volume and brightness.

## FAQ

The hardware RTC retains a valid calendar across resets independently of network time; an invalid calendar is initialized to 2025-01-01 12:00:00. Connect the battery before USB for battery measurements: hot-plug detection and forced charge-level re-estimation are not implemented, gauge temperature is not ambient temperature, and Music, Mic Test and AIChats must release their shared audio session before switching applications.
