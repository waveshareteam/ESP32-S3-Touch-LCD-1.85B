# 出厂固件

[English](README.md) | [简体中文](README_ZH.md)

下载适用于 ESP32-S3-Touch-LCD-1.85B 的合并固件，从 `0x0` 烧录。体验当前桌面及外设应用请选择最新版，早期出厂演示也可在下表下载。

| 文件 | 用途 |
| --- | --- |
| [ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20261109.bin](ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20261109.bin) | 当前出厂应用：新增电子徽章、幸运转盘、时钟表盘，以及统一的配网与传图网页入口，保留原有应用 |
| [ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20261027.bin](ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20261027.bin) | 上一版出厂应用：圆屏桌面、设置、媒体播放、外设测试和语音对话 |
| [ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20260328.bin](ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20260328.bin) | 早期出厂演示 |

## 烧录

执行 `python -m pip install esptool` 安装工具，通过 USB 连接开发板，并将 `<PORT>` 替换为开发板串口。擦除会清除板上保存的设置和徽章图片，请先备份所需数据。

```sh
python -m esptool --chip esp32s3 --port <PORT> erase_flash
python -m esptool --chip esp32s3 --port <PORT> write_flash 0x0 ESP32-S3-Touch-LCD-1.85B_FactoryOnly_20261109.bin
```

最新版镜像不包含存放上传徽章图片的 `assets` SPIFFS 分区，语音模型仍包含在内。如需保留板上已有图片，请跳过 `erase_flash`。

烧录后复位开发板即可运行。当前版本的源码和使用方法见 [examples/factory](../examples/factory/README_ZH.md)，早期固件包含的应用与当前版本不同。
