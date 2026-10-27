# ESP32-S3-Touch-LCD-1.85B Xiaozhi APP

Connect to Wi-Fi in Settings, then open AIChats from the desktop. On first
use, follow the activation instructions and bind the code shown on screen.
Once ready, say "你好小智" or tap Talk to start a conversation. Tap Stop to
end the current turn, or use the back arrow to return to the desktop.
Xiaozhi runs in the factory application without switching firmware or rebooting.

The 360x360 interface displays connection status, activation codes, expressions,
and conversation text. Long replies and activation instructions can be scrolled.
The ES8311 drives the speaker. Xiaozhi uses ES7210 MIC1 and MIC2 for voice and MIC3
for the hardware playback reference, with acoustic echo cancellation (AEC).
Say "你好小智" during a reply to interrupt it and start a new listening turn.
The server receives mono voice audio. Music, Mic Test and Xiaozhi share one
exclusive audio session. Exit an audio APP before opening another. Mic Test
records both microphones as 16 kHz PCM16 in PSRAM for up to 5 seconds. MIC1 and MIC2 are replayed separately through the speaker; no SD card is required.

Build with ESP-IDF 5.5.4. The 16 MiB partition table contains a single factory
application, a 1 MiB speech-model data partition at 0x9c0000, and the existing
assets partition at 0xac0000. The model partition contains raw ESP-SR model
data and is not mounted as a filesystem.

Read-only fonts and images remain in Flash to leave PSRAM available for the
desktop, audio buffers, and speech processing. Instruction fetching from PSRAM
remains enabled.

For the first installation, run `idf.py flash` so the bootloader, partition
table, application, and `build/srmodels/srmodels.bin` are all written. Flashing
only `01_factory.bin` will not install the new partition table or wake model.
No separate Xiaozhi application image is required.
