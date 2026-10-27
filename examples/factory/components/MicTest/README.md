# Mic Test

[English](README.md) | [简体中文](README_ZH.md)

## Function

Display separate MIC1/MIC2 levels, record both microphones for up to five seconds and replay each channel through the speaker.

## Hardware

ESP32-S3-Touch-LCD-1.85B and a connected speaker. An SD card is not required.

## Run

Install the [factory application](../../README.md), exit any other audio application and open Mic Test. Tap Record, speak near each microphone in turn, and tap Stop to finish early if needed.

## Expected behavior

Both microphone meters respond to sound; MIC1 and MIC2 buttons replay their corresponding recordings. Settings controls playback volume; a new recording replaces the previous take, and closing the application discards it.

## FAQ

The dBFS meters are digital recording levels, not calibrated sound-pressure measurements; CLIPPED indicates samples near digital full scale. This test checks raw microphones without echo cancellation or noise suppression; sound reaching both microphones is normal.
