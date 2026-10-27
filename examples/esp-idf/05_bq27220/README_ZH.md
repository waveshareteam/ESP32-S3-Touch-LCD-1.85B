# BQ27220 电池测试

[English](README.md) | [简体中文](README_ZH.md)

## 功能

使用出厂 500mAh 电池参数，读取电量、电压、电流、容量和电量计状态。

## 硬件需求

ESP32-S3-Touch-LCD-1.85B 开发板和 USB 数据线。 接入配套 3.7V、500mAh 电池。

## 怎么运行

使用 ESP-IDF v5.5.4，在当前示例目录运行：

```sh
idf.py set-target esp32s3 build flash monitor
```

## 预期现象

串口每秒更新一次，电流正值表示充电、负值表示放电。

## 常见问题

请先接好配套电池再连接 USB；本板 BIN 固定下拉，仅接 USB 或运行中插拔电池时，读数不能可靠判断电池在位和实际电量；温度为电量计内部温度。
