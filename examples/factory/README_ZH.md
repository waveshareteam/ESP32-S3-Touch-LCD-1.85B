# ESP32-S3-Touch-LCD-1.85B 出厂源码

[English](README.md) | [简体中文](README_ZH.md)

## 功能

提供适配 360×360 圆屏的完整桌面，每页四个图标；第一页依次为 Display Test、Settings、AIChats、Battery。包含触摸测试、相册、天气、音乐、RTC/陀螺仪、2048、双麦测试、按键测试、AVI/MJPEG 视频和重力游戏；AIChats 在 APP 内运行，无需切换固件分区。

## 硬件需求

ESP32-S3-Touch-LCD-1.85B（16MB Flash、8MB PSRAM），扬声器及配套 3.7V、500mAh 电池；网络功能需要 2.4GHz Wi-Fi。相册、音乐和视频需要 FAT 格式的 microSD 卡，文件分别放在 `photo/`、`music/`、`video/`，支持 JPEG、MP3 和帧尺寸不大于 360×360 的 AVI/MJPEG 视频（视频不播放音轨）。

## 怎么运行

使用 ESP-IDF v5.5.4，在本目录执行：

```sh
idf.py build flash monitor
```

在 Settings 内扫描并连接 Wi-Fi，或连接 `S3-LCD-1.85B-xxxxxx` 热点（密码 `12345678`），打开 `http://192.168.4.1` 配网；未保存配置时默认尝试连接 `weather-wifi` / `12345678`。

## 预期现象

首帧完成后开启背光，左右滑动切换桌面；相册居中裁剪铺满圆屏，视频预览区域为 240×240。双麦测试可分别查看 MIC1/MIC2、录制最多 5 秒并单独回放，录音存于 PSRAM；AIChats 使用双麦和 MIC3 回声参考，可唤醒、打断语音回复，并通过语音调节音量和亮度。

## 常见问题

RTC 独立走时，有效日期跨复位保留，网络对时只更新桌面时钟；无效 RTC 日历初始化为 2025-01-01 12:00:00。电量测试请先接电池再接 USB；当前不实现电池热插拔检测或主动重新估算，电量计温度不是环境温度；音乐、录音和 AIChats 共用音频资源，切换前先退出当前音频 APP。
