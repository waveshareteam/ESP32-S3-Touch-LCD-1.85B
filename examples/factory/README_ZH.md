# 出厂应用

[English](README.md) | [简体中文](README_ZH.md)

## 功能

提供适配 360 × 360 圆屏的桌面，每页四个图标：Display Test、Settings、AIChats、Battery、Touch Test、Photos、Weather、Music、Sensor、2048、Mic Test、Button Test、Video 和 Gravity。左右滑动切换页面，打开 AIChats 即可使用语音对话。

## 硬件需求

ESP32-S3-Touch-LCD-1.85B、扬声器及配套 3.7V、500mAh 电池；联网功能需要 2.4GHz Wi-Fi。媒体功能需要 FAT32 microSD 卡，JPEG 图片放入 `photo/`，MP3 音频放入 `music/`，AVI/MJPEG 视频放入 `video/`（帧尺寸不大于 360 × 360，视频不播放音轨）。

## 怎么运行

直接体验请按[固件说明](../../firmware/README_ZH.md)下载并烧录。自行编译请使用 ESP-IDF v5.5.4，在本目录执行：

```sh
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

在 Settings 中扫描并连接 Wi-Fi，或连接 `S3-LCD-1.85B-xxxxxx` 热点（密码 `12345678`），打开 `http://192.168.4.1` 配网。联网后打开 AIChats，如出现激活提示，按界面说明完成操作。

## 预期现象

桌面显示时间、Wi-Fi 和电池状态；相册铺满圆屏，视频预览区域为 240 × 240。Mic Test 分别显示 MIC1/MIC2 音量条，可录制最多 5 秒并单独回放；AIChats 支持语音对话、唤醒词打断，以及语音调节音量和亮度。

## 常见问题

Sensor 的 RTC 日历独立走时，网络对时更新桌面时钟。电量测试请先接电池再接 USB，仅 USB 供电或运行中插拔电池时读数可能不准确，电量计温度不代表环境温度；录音为临时保存，使用另一个音频应用前请先退出当前应用。
