# xiaozhi-esp32 adaptations

This directory contains the small OGG demuxer imported from
78/xiaozhi-esp32 at commit
0ec696f64f5843ca0f5fcf700ae45977d1dcd2e8.

Optional Chinese activation prompt and digit recordings can be installed in
`/spiffs/xiaozhi/` at image-generation time:

- activation.ogg
- 0.ogg through 9.ogg
- success.ogg

Those recordings are not bundled by this component. When present, they must
be 16 kHz mono Opus audio in OGG containers. The firmware demuxes each file
and feeds raw Opus packets to esp_audio_codec, avoiding a dependency on the
complete upstream audio framework. When they are absent, activation remains
fully usable through the on-screen activation code and message; prompt
playback is simply skipped.

The XiaozhiApp application also adapts the upstream WebSocket protocol,
wake-word Opus pre-roll, automatic listening-turn flow, and dynamic Opus
playback configuration from commit
1b48ebd7863695bf80d384c6c09af6299a6d7d0e. The local integration keeps the
Brookesia application lifecycle, board codec access, dual-microphone AFE,
and UI instead of importing the complete upstream board and application
framework. This ESP32-S3-Touch-LCD-1.85B port uses ES8311 playback and
ES7210 MIC1 and MIC2 plus the MIC3 hardware playback reference. Four 16-bit ADC slots
are packed into stereo 32-bit I2S words (raw reference slot 0, MIC1 slot 1, MIC2 slot 3).
Microphone and reference are resampled together from 24 kHz to 16 kHz and
fed to ESP-SR as "MMR". AEC and WakeNet remain active during replies for
wake-word interruption. Conversation audio sent to the server remains mono.
Music and recording use their original two-channel 16-bit audio configuration.

The imported OGG demuxer is distributed under the MIT license in `LICENSE`.
Any optional prompt recordings must be reviewed and licensed separately by
the firmware distributor before they are added to a filesystem image.
