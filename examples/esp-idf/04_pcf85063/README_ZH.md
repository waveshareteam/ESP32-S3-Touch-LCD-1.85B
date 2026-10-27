# PCF85063 RTC 测试

[English](README.md) | [简体中文](README_ZH.md)

## 功能

读取独立于网络时间的硬件 RTC 日历，复位时保留有效时间。

## 硬件需求

ESP32-S3-Touch-LCD-1.85B 开发板和 USB 数据线。

## 怎么运行

使用 ESP-IDF v5.5.4，在当前示例目录运行：

```sh
idf.py set-target esp32s3 build flash monitor
```

## 预期现象

串口每秒输出日期和时间；日历无效或振荡器停止时，BSP 初始化为 2025-01-01 12:00:00。

## 常见问题

如需演示设置时间，可修改 main/main.cpp 中的 SET_TIME_ON_BOOT 和初始日期；断电走时需要 RTC 电源持续供电。
