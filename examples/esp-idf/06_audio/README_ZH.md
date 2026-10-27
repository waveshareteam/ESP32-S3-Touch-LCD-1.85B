# 双麦克风与音频测试

[English](README.md) | [简体中文](README_ZH.md)

## 功能

分别显示 MIC1/MIC2 音量条，将双麦录音保存在 PSRAM，并通过扬声器单独回放各个麦克风。

## 硬件需求

ESP32-S3-Touch-LCD-1.85B 开发板和 USB 数据线。 回放测试需要接好扬声器。

## 怎么运行

使用 ESP-IDF v5.5.4，在当前示例目录运行：

```sh
idf.py set-target esp32s3 build flash monitor
```

## 预期现象

点击 Record 最多录制 5 秒，随后点击 MIC1 或 MIC2 回放对应通道，Stop 可提前停止录音或回放。

## 常见问题

不需要 SD 卡；录音为 16kHz 双声道 PCM16，断电即丢失；录音时关闭扬声器 PA，回放音量为 60%，不启动唤醒词或 AEC 处理。
