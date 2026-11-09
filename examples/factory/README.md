# Factory Application

[English](README.md) | [简体中文](README_ZH.md)

## Function

A 360 × 360 round-screen desktop with four icons per page: Display Test, Settings, AIChats, Battery, Touch Test, Photos, Weather, Music, Sensor, 2048, Mic Test, Button Test, Video, Gravity, Badge, Lucky Wheel and Watch. Swipe left or right to change pages; voice chat opens directly in AIChats.

## Hardware

ESP32-S3-Touch-LCD-1.85B, a speaker and the supplied 3.7 V, 500 mAh battery; online features need 2.4 GHz Wi-Fi. For media, insert a FAT32 microSD card with JPEG images in `photo/`, MP3 files in `music/`, and AVI/MJPEG videos in `video/` (frames up to 360 × 360; video audio tracks are not played).

## Run

For a ready-to-flash image, follow the [firmware guide](../../firmware/README.md). To build this application, use ESP-IDF v5.5.4 and run:

```sh
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

Open the phone connection entry in Settings and scan its Wi-Fi QR code, or manually join the `S3-LCD-1.85B-xxxxxx` hotspot with password `12345678`. Open `http://192.168.4.1`; the device home offers **Wi-Fi Setup** and **Transfer Images**. Some phones open the page automatically after joining. Open AIChats after connecting the device to your network and follow its activation instructions if prompted.

## Expected behavior

The desktop shows time, Wi-Fi and battery status; Photos fills the round screen, and Video displays a 240 × 240 preview. Mic Test shows separate MIC1/MIC2 levels and records up to five seconds for per-channel playback; AIChats supports voice conversation, wake-word interruption, and volume and brightness commands.

## FAQ

### One hotspot, two web tools

The root address always shows the device home. **Wi-Fi Setup** opens `/wifi`,
where you can scan nearby 2.4 GHz networks, select one (or enter a hidden SSID),
and save its credentials. **Transfer Images** opens `/badge` and initializes the
image manager directly; the Badge APP does not need to be open on the device.
Both pages provide a route back to the device home.

The QR code joins the device hotspot; it is not a second webpage QR code.
Stay connected even if your phone reports no Internet. After Wi-Fi setup,
the hotspot remains available for image transfer. Leaving a device screen does
not interrupt a browser-owned editor. Finish with **Done** in the image page
before switching to Wi-Fi setup. Choose **Disconnect & close hotspot** on the
home page to stop the hotspot. A browser session expires after ten minutes
without requests; reopen the phone connection entry if needed. Wi-Fi setup and
image transfer share one HTTP server, and scans/configuration are blocked while
the image editor is active to avoid disrupting a transfer.

### Electronic badge

Badge uses a consistent warm ivory and sage-green theme for its welcome page,
controls, phone connection instructions and web image manager.
Open **Badge** for a full-screen image. With no saved images, tap **Add images**
on the welcome page. Short tap the image and choose **Manage images**;
connect your phone to the hotspot shown on the device (password `12345678`), then
open `http://192.168.4.1` and choose **Transfer Images**. Choose one of six slots, select a JPEG, PNG or WebP,
drag and zoom the circular preview, and upload it. The browser converts the crop
to a 360 × 360 JPEG. You can select or delete saved images from the same page.
Choose **Done** on the web page to finish editing and return to the device home.
The device's **Done** button returns to its image view while an active phone
editor can continue. You can also enter the web manager from Settings' hotspot.
Images stay on screen until you switch them; there is no automatic slideshow.
Swipe left/right to switch between occupied slots, wrapping at either end. With
only one saved image, swiping keeps that image. Tap the image to open controls
with a circular photo preview, an image count and page dots. Use the left/right
arrow buttons to switch images, **Manage images** to edit, or **Exit** to leave.
Choose **Close** to hide the controls, or swipe up from the bottom edge to exit
while the image is visible. Images and the selected slot survive a normal restart, with no SD card
or Internet required. Uploading through the web manager does not require opening
Badge on the device; open Badge afterwards to display the saved image.
Holding the screen does not open the controls, including when you release it.

Badge mounts the existing 4 MB `assets` SPIFFS partition at `/badge`, formatting it
on first use if it has no valid filesystem. Application/model partition offsets
are unchanged. A full flash erase or overwriting `assets` removes badge images.
Uploads are limited to 256 KB each and validated before replacing a saved image.
The web editor is adapted from [M5Stack M5StopWatch-UserDemo](https://github.com/m5stack/M5StopWatch-UserDemo/tree/81099c784991dc77f522de1a5bf62ec5de9ae0b5/main/apps/app_badge);
its MIT notice is preserved in `components/Badge/LICENSE-M5Stack`.
The launcher icon uses the product's silver circular shell with an illustrated
photo on its display. Regenerate it with `python components/Badge/assets/generate_icon.py`
(requires Pillow and the included Waveshare `badge-device-shell.png` reference).

### Lucky Wheel

Open **Lucky Wheel** and short tap the center **SPIN** button. The full-screen
pastel wheel fills the round panel to its edge without an outer border, spins
in a random direction, slows down over 2.4–3.6 seconds, and
shows the number under the fixed top pointer. Tap the center to spin again.
All choices have equal probability; repeated taps during a spin are ignored.
Numbers stay upright while moving with their sectors for readability.

Tap **Options** to choose 2–18 numbered sectors (8 by default), then **Done**
to apply. The count remains selected until restart; the app does not write to
Flash. Choose **Exit to desktop**, swipe up from the bottom edge of the wheel,
or press BOOT to exit. No network or SD card is required.

Spin behavior is adapted from [M5Stack M5StopWatch-UserDemo](https://github.com/m5stack/M5StopWatch-UserDemo/tree/81099c784991dc77f522de1a5bf62ec5de9ae0b5/main/apps/app_lucky_wheel);
its MIT notice is preserved in `components/LuckyWheel/LICENSE-M5Stack`.
The interface and launcher icon are drawn for this product. Regenerate the
112 × 112 icon with `python components/LuckyWheel/assets/generate_icon.py`
(requires Pillow).

### Watch

Open **Watch** for an edge-to-edge ivory analog dial, twelve gray markers,
charcoal hour/minute hands and a sweeping red second hand, matching its launcher icon. Short tap to cycle
through the clean dial, time/date details and hands-only views. Swipe up from
the bottom edge or press BOOT to exit. No SD card is required.

At boot, the network worker restores the system clock from the hardware RTC.
After Wi-Fi connects, SNTP corrects it to Beijing time (UTC+8) and the worker
writes the corrected local calendar back to the RTC. Watch and the desktop
read the same system clock; Sensor shows the hardware calendar. Disconnecting
Wi-Fi does not stop the clock. The details view shows **RTC** before network
synchronization and **SYNCED** after it, even if Wi-Fi later disconnects.
If neither source is available, the app shows **Waiting for time** instead of
inventing a time. An RTC reset or loss of backup power can leave a default date;
connect Wi-Fi once to set the correct time. Offline accuracy depends on RTC
power retention and drift. The timezone is fixed to UTC+8.

Classic-face behavior is adapted from [M5Stack M5StopWatch-UserDemo](https://github.com/m5stack/M5StopWatch-UserDemo/tree/81099c784991dc77f522de1a5bf62ec5de9ae0b5/main/apps/app_watch_face);
the MIT notice is preserved in `components/Watch/LICENSE-M5Stack`.
The dial and launcher artwork use native drawing for this product. Regenerate
the icon with `python components/Watch/assets/generate_icon.py` (Pillow).

### Other applications

Connect the battery before USB for battery measurements: USB-only or hot-plug readings can be inaccurate, and gauge temperature is not ambient temperature; recordings are temporary, and you should exit the current audio application before opening another.
