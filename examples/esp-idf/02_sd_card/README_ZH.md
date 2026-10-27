# SD 卡读写与屏幕显示

[English](README.md) | [简体中文](README_ZH.md)

## 功能

验证 microSD 文本文件写入和读回，在 360 × 360 圆屏显示测试结果和文件内容。串口同时输出卡容量及剩余空间。

## 硬件需求

ESP32-S3-Touch-LCD-1.85B 开发板、USB 数据线和 FAT32 microSD 卡。上电前插卡，每次测试仅覆盖卡根目录的 `test.txt`，不会格式化 SD 卡。

## 怎么运行

使用 ESP-IDF v5.5.4，在当前示例目录执行以下命令；烧录后自动开始测试：

```sh
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## 预期现象

屏幕显示 **Write / read passed**、`ESP_OK`、`/sdcard/test.txt` 和文件内容 `ESP32-S3-Touch-LCD-1.85B SD test`。测试后卸载 SD 卡，按 RESET 可再次测试。

## 常见问题

挂载、写入、读取或卸载失败时，屏幕保留失败阶段和重测提示，详情见串口日志。插好可写的 FAT32 卡后按 RESET 重试；仅插卡不会自动重测。
