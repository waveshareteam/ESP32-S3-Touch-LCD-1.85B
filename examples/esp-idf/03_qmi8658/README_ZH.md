# QMI8658 测试

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过 BSP 读取三轴加速度、角速度和 IMU 内部温度。

## 硬件需求

ESP32-S3-Touch-LCD-1.85B 开发板和 USB 数据线。

## 怎么运行

使用 ESP-IDF v5.5.4，在当前示例目录运行：

```sh
idf.py set-target esp32s3 build flash monitor
```

## 预期现象

串口每 500 ms 更新一次加速度（g）和角速度（度/秒），转动开发板可观察变化。

## 常见问题

温度为 IMU 芯片内部温度，不是环境温度。
