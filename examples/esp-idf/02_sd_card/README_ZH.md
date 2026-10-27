# SD 卡测试

[English](README.md) | [简体中文](README_ZH.md)

## 功能

挂载四线 SDMMC 卡，输出容量和剩余空间，并验证文件写入和读回。

## 硬件需求

ESP32-S3-Touch-LCD-1.85B 开发板和 USB 数据线。 需要 FAT32 格式的 microSD 卡。

## 怎么运行

使用 ESP-IDF v5.5.4，在当前示例目录运行：

```sh
idf.py set-target esp32s3 build flash monitor
```

## 预期现象

串口输出卡容量及读写 PASS，测试完成后卸载 SD 卡。

## 常见问题

复位前插入 FAT32 格式的 microSD 卡；测试仅覆盖 /bsp_sd_test.txt，不会格式化存储卡。
