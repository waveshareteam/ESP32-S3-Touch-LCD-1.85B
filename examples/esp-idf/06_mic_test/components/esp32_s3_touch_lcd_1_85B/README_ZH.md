# 板级支持

[English](README.md) | [简体中文](README_ZH.md)

## 功能

提供 360×360 ST77916 圆屏、CST816 触摸、ES8311/ES7210 音频、PCF85063 RTC、QMI8658、BQ27220、BOOT 和四线 SDMMC 接口。

## 硬件需求

ESP32-S3-Touch-LCD-1.85B，引脚统一定义在 `include/bsp/esp32_s3_touch_lcd_1_85B.h`。

## 怎么运行

包含 `bsp/esp-bsp.h`，只初始化示例需要的外设；出厂工程整体启动使用 `bsp_init()`，可通过 `bsp_get_init_status()` 查看可选外设状态。

## 预期现象

`bsp_display_show()` 在首帧完成后开启背光，跨任务访问显示需要 BSP 锁。RTC 和电量数据来自板载外设，音频会话及 PCM 适配由 `bsp_extra` 管理。

## 常见问题

先完成硬件初始化，再启动工作任务；文件操作期间持有 SD 租约，音频操作结束后释放会话；外设句柄归 BSP 所有。
