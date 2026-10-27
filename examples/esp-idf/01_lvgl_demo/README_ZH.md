# LVGL 触摸示例

[English](README.md) | [简体中文](README_ZH.md)

## 功能

显示适配圆屏的简单界面，演示触摸计数和背光亮度滑块。

## 硬件需求

ESP32-S3-Touch-LCD-1.85B 开发板和 USB 数据线。

## 怎么运行

使用 ESP-IDF v5.5.4，在当前示例目录运行：

```sh
idf.py set-target esp32s3 build flash monitor
```

## 预期现象

点击按钮后计数增加，拖动滑块可调节背光亮度。

## 常见问题

暂无。
