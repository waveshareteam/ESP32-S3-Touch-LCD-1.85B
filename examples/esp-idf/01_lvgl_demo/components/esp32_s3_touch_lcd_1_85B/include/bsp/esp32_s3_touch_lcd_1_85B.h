#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_lv_adapter.h"
#include "bsp/config.h"
#include "bsp/display.h"
#include "bsp/touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Board pin assignments. GPIO_NUM_NC means no dedicated MCU GPIO is assigned. */

/* Shared I2C: CST816 touch, ES8311 DAC, ES7210 ADC, PCF85063 RTC and QMI8658 IMU. */
#define BSP_I2C_NUM             I2C_NUM_1
#define BSP_I2C_SDA             GPIO_NUM_11
#define BSP_I2C_SCL             GPIO_NUM_10
#define BSP_I2C_FREQ_HZ         400000

/* ST77916 LCD: four-wire QSPI data interface and PWM backlight. */
#define BSP_LCD_SPI_HOST        SPI2_HOST
#define BSP_LCD_SCLK            GPIO_NUM_40
#define BSP_LCD_DATA0           GPIO_NUM_46
#define BSP_LCD_DATA1           GPIO_NUM_45
#define BSP_LCD_DATA2           GPIO_NUM_42
#define BSP_LCD_DATA3           GPIO_NUM_41
#define BSP_LCD_CS              GPIO_NUM_21
#define BSP_LCD_RST             GPIO_NUM_3
#define BSP_LCD_BACKLIGHT       GPIO_NUM_5

/* CST816 capacitive touch: coordinates are read on the shared I2C bus. */
#define BSP_TOUCH_RST           GPIO_NUM_1
#define BSP_TOUCH_INT           GPIO_NUM_4

/* BOOT button: active-low input. */
#define BSP_BUTTON_BOOT         GPIO_NUM_0

/* MicroSD card: independent four-bit SDMMC interface. */
#define BSP_SD_CLK              GPIO_NUM_15
#define BSP_SD_CMD              GPIO_NUM_14
#define BSP_SD_D0               GPIO_NUM_16
#define BSP_SD_D1               GPIO_NUM_17
#define BSP_SD_D2               GPIO_NUM_12
#define BSP_SD_D3               GPIO_NUM_13
#define BSP_SD_MOUNT_POINT      "/sdcard"

/* ES8311 playback and ES7210 capture share the I2S clocks.
 * DOUT/DIN directions are relative to the MCU. */
#define BSP_I2S_NUM             I2S_NUM_1
#define BSP_I2S_MCLK            GPIO_NUM_2
#define BSP_I2S_BCLK            GPIO_NUM_48
#define BSP_I2S_LRCLK           GPIO_NUM_38
#define BSP_I2S_DOUT            GPIO_NUM_47
#define BSP_I2S_DIN             GPIO_NUM_39
#define BSP_AUDIO_PA            GPIO_NUM_9

#define BSP_CAPS_DISPLAY 1
#define BSP_CAPS_TOUCH 1
#define BSP_CAPS_BUTTONS 1
#define BSP_CAPS_AUDIO 1
#define BSP_CAPS_AUDIO_SPEAKER 1
#define BSP_CAPS_AUDIO_MIC 1
#define BSP_CAPS_SDCARD 1
#define BSP_CAPS_IMU 1
#define BSP_CAPS_CAMERA 0

esp_err_t bsp_i2c_init(void);
esp_err_t bsp_i2c_deinit(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

/**************************************************************************************************
 * I2S audio interface. Handles are BSP-owned; stop all users before deinit.
 **************************************************************************************************/
#define BSP_AUDIO_DMA_DESCRIPTORS 6
#define BSP_AUDIO_DMA_FRAMES 240
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config);
esp_err_t bsp_audio_init_voice_24k(void);
esp_err_t bsp_audio_deinit(void);
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void);
/* ES7210-specific four-slot capture with MIC3 as the playback reference. */
esp_codec_dev_handle_t bsp_audio_codec_voice_microphone_init(void);
/* Bounded I2S access for full-duplex voice, after opening the shared codecs. */
esp_err_t bsp_audio_i2s_read(void *data, size_t size, size_t *read, uint32_t timeout_ms);
esp_err_t bsp_audio_i2s_write(void *data, size_t size, size_t *written, uint32_t timeout_ms);

/* Keep the display adapter required by the original AVI/MJPEG player.
 * Buffer size is expressed in pixels, as in the S3-3.5 display configuration. */
typedef struct {
    esp_lv_adapter_config_t lv_adapter_cfg;
    uint32_t buffer_size;
    bool double_buffer;
    struct {
        unsigned int buff_spiram: 1;
    } flags;
} bsp_display_cfg_t;

lv_display_t *bsp_display_start(void);
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);
lv_indev_t *bsp_display_get_input_dev(void);
bool bsp_display_lock(uint32_t timeout_ms);
void bsp_display_unlock(void);
void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation);

#ifdef __cplusplus
}
#endif

#include "bsp/board.h"

