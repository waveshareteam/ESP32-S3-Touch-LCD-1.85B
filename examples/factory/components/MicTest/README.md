# Mic Test

The round-screen test shows separate MIC1 and MIC2 levels in dBFS. Digital levels are uncalibrated references, not sound-pressure readings.

- Tap Record to capture both microphones together for up to 5 seconds. Tap Stop to finish earlier and keep the completed portion.
- Tap MIC1 or MIC2 to play only that microphone's recording through the speaker. The other microphone is not mixed in. Stop ends playback.
- A new recording replaces the previous take. A completed take remains available after pausing and resuming the APP; closing the APP discards it.
- Recordings use 320,000 bytes of PSRAM at 16 kHz, two channels, PCM16. No SD card or filesystem is used.
- CLIPPED indicates samples near digital full scale. It stays visible until a new recording or a new APP session. Compare the two recordings by speaking near each microphone; sound reaching both microphones is normal.
- Playback uses the shared volume from Settings. The test does not automatically raise it.

The recording and playback state machine follows the RLCD-4.2 Mic Test. The screen layout and board audio access are adapted for the 360 × 360 circular 1.85B display.

On this board, raw testing uses the ES7210 two-input MIC1/MIC2 mode: slot 0 is MIC1 and slot 1 is MIC2. MIC3 is excluded. Both inputs use a fixed 24 dB gain. No AFE, AEC, automatic gain, noise suppression, or wake-word processing is applied. The first 200 ms after opening capture is discarded.

Capture keeps the speaker muted and PA disabled. Playback closes capture, opens output, and duplicates only the selected microphone into both output slots. Capture resumes after playback. Mic Test owns the Recorder audio session, independently of Xiaozhi; only one audio APP can use the codecs at a time. Codec cleanup completes before the worker is deleted.

This test checks raw microphone recording and individual replay. AEC behavior belongs to Xiaozhi and is not evaluated here.
