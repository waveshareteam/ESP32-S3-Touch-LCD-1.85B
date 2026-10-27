# Mic Test

[English](README.md) | [简体中文](README_ZH.md)

## Function

Display separate MIC1/MIC2 levels, record both microphones into PSRAM, and replay each microphone separately through the speaker.

## Hardware

ESP32-S3-Touch-LCD-1.85B and a USB cable. Connect a speaker to hear playback.

## Run

Use ESP-IDF v5.5.4 from this example directory:

```sh
idf.py set-target esp32s3 build flash monitor
```

## Expected behavior

Tap Record to capture up to five seconds, then tap MIC1 or MIC2 to hear that channel. Stop ends recording or playback early.

## FAQ

No SD card is required. Recordings are volatile 16 kHz stereo PCM16; capture keeps the speaker PA off, playback uses 60% volume, and no wake-word or AEC processing is started.
