#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "bsp_board_internal.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_codec_dev_defaults.h"
#include "esp_lcd_st77916.h"
#include "esp_lcd_touch_cst816s.h"
#include "driver/ledc.h"
#include "driver/sdmmc_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "bsp_1_85B";
static i2c_master_bus_handle_t i2c_bus;
static esp_lcd_panel_io_handle_t lcd_io, touch_io;
static esp_lcd_panel_handle_t panel;
static esp_lcd_touch_handle_t touch;
static lv_display_t *display;
static lv_indev_t *input;
static bool backlight_ready, display_started, port_initialized;
static bool backlight_released;
static int brightness = CONFIG_BSP_LCD_BRIGHTNESS;
static SemaphoreHandle_t sd_mutex;
static sdmmc_card_t *sdcard;
static unsigned sd_leases;

/**************************************************************************************************
 * I2C interface
 **************************************************************************************************/
esp_err_t bsp_i2c_init(void)
{
    if (i2c_bus) return ESP_OK;
    ESP_RETURN_ON_ERROR(bsp_board_init_locks(), TAG, "Board locks");
    if (!sd_mutex) sd_mutex = xSemaphoreCreateMutex();
    if (!sd_mutex) return ESP_ERR_NO_MEM;
    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = BSP_I2C_NUM;
    cfg.sda_io_num = BSP_I2C_SDA;
    cfg.scl_io_num = BSP_I2C_SCL;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = true;
    return i2c_new_master_bus(&cfg, &i2c_bus);
}

esp_err_t bsp_i2c_deinit(void)
{
    if (!i2c_bus) return ESP_OK;
    // Child devices must be removed before deleting their common bus.
    if (touch || bsp_board_i2c_in_use()) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = i2c_del_master_bus(i2c_bus);
    if (ret == ESP_OK) i2c_bus = nullptr;
    return ret;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    return bsp_i2c_init() == ESP_OK ? i2c_bus : nullptr;
}

static esp_err_t sdcard_mount_locked(void)
{
    if (sdcard) return ESP_OK;
    esp_vfs_fat_sdmmc_mount_config_t cfg = {};
    cfg.format_if_mount_failed = false;
    cfg.max_files = 8;
    cfg.allocation_unit_size = 16 * 1024;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = BSP_SD_CLK;
    slot.cmd = BSP_SD_CMD;
    slot.d0 = BSP_SD_D0;
    slot.d1 = BSP_SD_D1;
    slot.d2 = BSP_SD_D2;
    slot.d3 = BSP_SD_D3;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(BSP_SD_MOUNT_POINT, &host, &slot, &cfg, &sdcard);
    if (ret != ESP_OK) sdcard = nullptr;
    bsp_board_status.sdcard = ret;
    return ret;
}

esp_err_t bsp_sdcard_mount(void)
{
    if (!sd_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    esp_err_t ret = sdcard_mount_locked();
    xSemaphoreGive(sd_mutex);
    return ret;
}

esp_err_t bsp_sdcard_acquire(void)
{
    if (!sd_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    esp_err_t ret = sdcard_mount_locked();
    if (ret == ESP_OK) ++sd_leases;
    xSemaphoreGive(sd_mutex);
    return ret;
}

void bsp_sdcard_release(void)
{
    if (!sd_mutex) return;
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    if (sd_leases) --sd_leases;
    xSemaphoreGive(sd_mutex);
}

esp_err_t bsp_sdcard_unmount(void)
{
    if (!sd_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    esp_err_t ret = sd_leases ? ESP_ERR_INVALID_STATE : ESP_OK;
    if (ret == ESP_OK && sdcard) {
        ret = esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, sdcard);
        if (ret == ESP_OK) { sdcard = nullptr; bsp_board_status.sdcard = ESP_ERR_INVALID_STATE; }
    }
    xSemaphoreGive(sd_mutex);
    return ret;
}

esp_err_t bsp_sdcard_get_info(bsp_sdcard_info_t *info)
{
    if (!info) return ESP_ERR_INVALID_ARG;
    *info = {};
    if (!sd_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(sd_mutex, portMAX_DELAY);
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (sdcard) {
        info->mounted = true;
        info->capacity_bytes = (uint64_t)sdcard->csd.capacity * sdcard->csd.sector_size;
        ret = esp_vfs_fat_info(BSP_SD_MOUNT_POINT, &info->total_bytes, &info->free_bytes);
    }
    xSemaphoreGive(sd_mutex);
    return ret;
}

/**************************************************************************************************
 * I2S audio interface: ES8311 playback and ES7210 microphone capture.
 **************************************************************************************************/
static i2s_chan_handle_t tx, rx;
static const audio_codec_data_if_t *data_if;
static const audio_codec_ctrl_if_t *ctrl_if;
static const audio_codec_gpio_if_t *gpio_if;
static const audio_codec_if_t *codec_if;
static esp_codec_dev_handle_t codec;
struct MicrophoneCodec {
    const audio_codec_ctrl_if_t *ctrl_if;
    const audio_codec_if_t *codec_if;
    esp_codec_dev_handle_t codec;
};
static MicrophoneCodec microphones[2];

bool bsp_board_audio_in_use(void) { return data_if || codec; }

static void bsp_audio_cleanup(void)
{
    for (auto &mic : microphones) {
        if (mic.codec) esp_codec_dev_delete(mic.codec);
        if (mic.codec_if) audio_codec_delete_codec_if(mic.codec_if);
        if (mic.ctrl_if) audio_codec_delete_ctrl_if(mic.ctrl_if);
        mic = {};
    }
    if (codec) { esp_codec_dev_delete(codec); codec = nullptr; }
    if (codec_if) { audio_codec_delete_codec_if(codec_if); codec_if = nullptr; }
    if (ctrl_if) { audio_codec_delete_ctrl_if(ctrl_if); ctrl_if = nullptr; }
    if (gpio_if) { audio_codec_delete_gpio_if(gpio_if); gpio_if = nullptr; }
    if (data_if) { audio_codec_delete_data_if(data_if); data_if = nullptr; }
    if (tx) { i2s_channel_disable(tx); i2s_del_channel(tx); tx = nullptr; }
    if (rx) { i2s_channel_disable(rx); i2s_del_channel(rx); rx = nullptr; }
}

esp_err_t bsp_audio_deinit(void)
{
    // All APP workers must be stopped before releasing these BSP-owned handles.
    bsp_audio_cleanup();
    return ESP_OK;
}

static esp_err_t bsp_audio_init_channels(const i2s_std_config_t *i2s_config)
{
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(BSP_I2S_NUM, I2S_ROLE_MASTER);
    channel.dma_desc_num = BSP_AUDIO_DMA_DESCRIPTORS;
    channel.dma_frame_num = BSP_AUDIO_DMA_FRAMES;
    channel.auto_clear_after_cb = true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&channel, &tx, &rx), TAG, "I2S channels");
    i2s_std_config_t cfg = {};
    if (i2s_config) cfg = *i2s_config;
    else {
        cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(22050);
        cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
        cfg.gpio_cfg.mclk = BSP_I2S_MCLK;
        cfg.gpio_cfg.bclk = BSP_I2S_BCLK;
        cfg.gpio_cfg.ws = BSP_I2S_LRCLK;
        cfg.gpio_cfg.dout = BSP_I2S_DOUT;
        cfg.gpio_cfg.din = BSP_I2S_DIN;
    }
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(tx, &cfg), TAG, "I2S TX");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(rx, &cfg), TAG, "I2S RX");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(tx), TAG, "I2S TX enable");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(rx), TAG, "I2S RX enable");
    audio_codec_i2s_cfg_t data = {};
    data.port = BSP_I2S_NUM;
    data.tx_handle = tx;
    data.rx_handle = rx;
    data_if = audio_codec_new_i2s_data(&data);
    return data_if ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    if (data_if) return ESP_OK;
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C bus");
    esp_err_t ret = bsp_audio_init_channels(i2s_config);
    if (ret != ESP_OK) bsp_audio_cleanup();
    return ret;
}

esp_err_t bsp_audio_init_voice_24k(void)
{
    i2s_std_config_t cfg = {};
    cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(24000);
    cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    cfg.gpio_cfg.mclk = BSP_I2S_MCLK;
    cfg.gpio_cfg.bclk = BSP_I2S_BCLK;
    cfg.gpio_cfg.ws = BSP_I2S_LRCLK;
    cfg.gpio_cfg.dout = BSP_I2S_DOUT;
    cfg.gpio_cfg.din = BSP_I2S_DIN;
    return bsp_audio_init(&cfg);
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    if (codec) return codec;
    if (bsp_audio_init(nullptr) != ESP_OK) return nullptr;
    // Codec control uses an 8-bit address; the bus probe takes 7 bits.
    if (i2c_master_probe(bsp_i2c_get_handle(), ES8311_CODEC_DEFAULT_ADDR >> 1, 100) != ESP_OK) return nullptr;
    audio_codec_i2c_cfg_t ctrl = {};
    ctrl.port = BSP_I2C_NUM;
    ctrl.addr = ES8311_CODEC_DEFAULT_ADDR;
    ctrl.bus_handle = bsp_i2c_get_handle();
    ctrl_if = audio_codec_new_i2c_ctrl(&ctrl);
    gpio_if = audio_codec_new_gpio();
    if (!ctrl_if || !gpio_if) goto failed;
    {
        es8311_codec_cfg_t chip = {};
        chip.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
        chip.ctrl_if = ctrl_if;
        chip.gpio_if = gpio_if;
        chip.pa_pin = BSP_AUDIO_PA;
        chip.use_mclk = true;
        chip.master_mode = false;
        chip.hw_gain.pa_voltage = 5.0f;
        chip.hw_gain.codec_dac_voltage = 3.3f;
        codec_if = es8311_codec_new(&chip);
        if (!codec_if) goto failed;
        esp_codec_dev_cfg_t dev = {};
        dev.dev_type = ESP_CODEC_DEV_TYPE_OUT;
        dev.codec_if = codec_if;
        dev.data_if = data_if;
        codec = esp_codec_dev_new(&dev);
        if (!codec) goto failed;
    }
    return codec;
failed:
    bsp_audio_cleanup();
    return nullptr;
}

static esp_codec_dev_handle_t bsp_audio_codec_microphone_new(unsigned index, uint8_t mic_selected)
{
    auto &mic = microphones[index];
    if (mic.codec) return mic.codec;
    if (bsp_audio_init(nullptr) != ESP_OK) return nullptr;
    audio_codec_i2c_cfg_t ctrl = {};
    ctrl.port = BSP_I2C_NUM;
    ctrl.addr = ES7210_CODEC_DEFAULT_ADDR;
    ctrl.bus_handle = bsp_i2c_get_handle();
    mic.ctrl_if = audio_codec_new_i2c_ctrl(&ctrl);
    if (!mic.ctrl_if) return nullptr;
    es7210_codec_cfg_t chip = {};
    chip.ctrl_if = mic.ctrl_if;
    chip.mic_selected = mic_selected;
    mic.codec_if = es7210_codec_new(&chip);
    if (mic.codec_if) {
        esp_codec_dev_cfg_t dev = {};
        dev.dev_type = ESP_CODEC_DEV_TYPE_IN;
        dev.codec_if = mic.codec_if;
        dev.data_if = data_if;
        mic.codec = esp_codec_dev_new(&dev);
    }
    if (!mic.codec) {
        if (mic.codec_if) audio_codec_delete_codec_if(mic.codec_if);
        audio_codec_delete_ctrl_if(mic.ctrl_if);
        mic = {};
    }
    return mic.codec;
}

esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    return bsp_audio_codec_microphone_new(0, 0);
}

esp_codec_dev_handle_t bsp_audio_codec_voice_microphone_init(void)
{
    // MIC3 carries the DAC reference. Three enabled inputs select four-slot TDM.
    return bsp_audio_codec_microphone_new(1, ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3);
}

esp_err_t bsp_audio_i2s_read(void *data, size_t size, size_t *read, uint32_t timeout_ms)
{
    return rx ? i2s_channel_read(rx, data, size, read, timeout_ms) : ESP_ERR_INVALID_STATE;
}
esp_err_t bsp_audio_i2s_write(void *data, size_t size, size_t *written, uint32_t timeout_ms)
{
    return tx ? i2s_channel_write(tx, data, size, written, timeout_ms) : ESP_ERR_INVALID_STATE;
}

esp_err_t bsp_display_brightness_init(void)
{
    if (backlight_ready) return ESP_OK;
    // Drive BL low before LCD reset or power configuration can expose blank GRAM.
    ESP_RETURN_ON_ERROR(gpio_set_level(BSP_LCD_BACKLIGHT, 0), TAG, "Backlight off level");
    ESP_RETURN_ON_ERROR(gpio_set_direction(BSP_LCD_BACKLIGHT, GPIO_MODE_OUTPUT), TAG, "Backlight off GPIO");
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_10_BIT;
    timer.timer_num = LEDC_TIMER_1;
    timer.freq_hz = 5000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "Backlight timer");
    ledc_channel_config_t channel = {};
    channel.gpio_num = BSP_LCD_BACKLIGHT;
    channel.speed_mode = LEDC_LOW_SPEED_MODE;
    channel.channel = LEDC_CHANNEL_1;
    channel.timer_sel = LEDC_TIMER_1;
    channel.duty = 0;
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "Backlight PWM");
    backlight_ready = true;
    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int percent)
{
    if (percent < 0 || percent > 100) return ESP_ERR_INVALID_ARG;
    if (!backlight_ready) return ESP_ERR_INVALID_STATE;
    const int duty = backlight_released ? percent * 1023 / 100 : 0;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty), TAG, "Backlight duty");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1), TAG, "Backlight update");
    brightness = percent;
    return ESP_OK;
}

int bsp_display_brightness_get(void) { return brightness; }

esp_err_t bsp_display_backlight_off(void) { return bsp_display_brightness_set(0); }

esp_err_t bsp_display_backlight_on(void) { return bsp_display_brightness_set(CONFIG_BSP_LCD_BRIGHTNESS); }

esp_err_t bsp_display_show(void)
{
    if (!display_started || !display || !lcd_io) return ESP_ERR_INVALID_STATE;
    if (!bsp_display_lock(0)) return ESP_ERR_TIMEOUT;
    lv_obj_invalidate(lv_display_get_screen_active(display));
    lv_refr_now(display);
    // Drain the final QSPI DMA transfer before releasing the initial backlight hold.
    esp_err_t ret = esp_lcd_panel_io_tx_param(lcd_io, -1, nullptr, 0);
    if (ret == ESP_OK) {
        backlight_released = true;
        ret = bsp_display_brightness_set(brightness);
        if (ret != ESP_OK) backlight_released = false;
    }
    bsp_display_unlock();
    if (ret == ESP_OK) ESP_LOGI(TAG, "Initial UI frame flushed; backlight enabled at %d%%", brightness);
    return ret;
}

static const uint8_t lcd_cmd_data_0[] = {0x28};
static const uint8_t lcd_cmd_data_1[] = {0x28};
static const uint8_t lcd_cmd_data_2[] = {0xD1};
static const uint8_t lcd_cmd_data_3[] = {0xE0};
static const uint8_t lcd_cmd_data_4[] = {0x61};
static const uint8_t lcd_cmd_data_5[] = {0x82};
static const uint8_t lcd_cmd_data_6[] = {0x00};
static const uint8_t lcd_cmd_data_7[] = {0x01};
static const uint8_t lcd_cmd_data_8[] = {0x01};
static const uint8_t lcd_cmd_data_9[] = {0x49};
static const uint8_t lcd_cmd_data_10[] = {0x4A};
static const uint8_t lcd_cmd_data_11[] = {0x1F};
static const uint8_t lcd_cmd_data_12[] = {0x46};
static const uint8_t lcd_cmd_data_13[] = {0x34};
static const uint8_t lcd_cmd_data_14[] = {0xD5};
static const uint8_t lcd_cmd_data_15[] = {0x30};
static const uint8_t lcd_cmd_data_16[] = {0x04};
static const uint8_t lcd_cmd_data_17[] = {0x00};
static const uint8_t lcd_cmd_data_18[] = {0x08};
static const uint8_t lcd_cmd_data_19[] = {0x08};
static const uint8_t lcd_cmd_data_20[] = {0x00};
static const uint8_t lcd_cmd_data_21[] = {0x80};
static const uint8_t lcd_cmd_data_22[] = {0x10};
static const uint8_t lcd_cmd_data_23[] = {0x37};
static const uint8_t lcd_cmd_data_24[] = {0x80};
static const uint8_t lcd_cmd_data_25[] = {0x10};
static const uint8_t lcd_cmd_data_26[] = {0x37};
static const uint8_t lcd_cmd_data_27[] = {0xA9};
static const uint8_t lcd_cmd_data_28[] = {0x41};
static const uint8_t lcd_cmd_data_29[] = {0x01};
static const uint8_t lcd_cmd_data_30[] = {0xA9};
static const uint8_t lcd_cmd_data_31[] = {0x41};
static const uint8_t lcd_cmd_data_32[] = {0x01};
static const uint8_t lcd_cmd_data_33[] = {0x91};
static const uint8_t lcd_cmd_data_34[] = {0x68};
static const uint8_t lcd_cmd_data_35[] = {0x68};
static const uint8_t lcd_cmd_data_36[] = {0x00, 0xA5};
static const uint8_t lcd_cmd_data_37[] = {0x35};
static const uint8_t lcd_cmd_data_38[] = {0x35};
static const uint8_t lcd_cmd_data_39[] = {0x3F};
static const uint8_t lcd_cmd_data_40[] = {0x3F};
static const uint8_t lcd_cmd_data_41[] = {0x10};
static const uint8_t lcd_cmd_data_42[] = {0x00};
static const uint8_t lcd_cmd_data_43[] = {0x02};
static const uint8_t lcd_cmd_data_44[] = {0x70, 0x09, 0x12, 0x0C, 0x0B, 0x27, 0x38, 0x54, 0x4E, 0x19, 0x15, 0x15, 0x2C, 0x2F};
static const uint8_t lcd_cmd_data_45[] = {0x70, 0x08, 0x11, 0x0C, 0x0B, 0x27, 0x38, 0x43, 0x4C, 0x18, 0x14, 0x14, 0x2B, 0x2D};
static const uint8_t lcd_cmd_data_46[] = {0xF0, 0x0E, 0x15, 0x0B, 0x0B, 0x07, 0x3C, 0x44, 0x51, 0x38, 0x15, 0x15, 0x32, 0x36};
static const uint8_t lcd_cmd_data_47[] = {0xF0, 0x0D, 0x15, 0x0A, 0x0A, 0x26, 0x3B, 0x43, 0x50, 0x37, 0x14, 0x15, 0x31, 0x36};
static const uint8_t lcd_cmd_data_48[] = {0x10};
static const uint8_t lcd_cmd_data_49[] = {0x10};
static const uint8_t lcd_cmd_data_50[] = {0x08};
static const uint8_t lcd_cmd_data_51[] = {0x00};
static const uint8_t lcd_cmd_data_52[] = {0x0B};
static const uint8_t lcd_cmd_data_53[] = {0x00};
static const uint8_t lcd_cmd_data_54[] = {0xE0};
static const uint8_t lcd_cmd_data_55[] = {0x06};
static const uint8_t lcd_cmd_data_56[] = {0x21};
static const uint8_t lcd_cmd_data_57[] = {0x00};
static const uint8_t lcd_cmd_data_58[] = {0x05};
static const uint8_t lcd_cmd_data_59[] = {0x82};
static const uint8_t lcd_cmd_data_60[] = {0xDF};
static const uint8_t lcd_cmd_data_61[] = {0x89};
static const uint8_t lcd_cmd_data_62[] = {0x20};
static const uint8_t lcd_cmd_data_63[] = {0x14};
static const uint8_t lcd_cmd_data_64[] = {0xFF};
static const uint8_t lcd_cmd_data_65[] = {0x00};
static const uint8_t lcd_cmd_data_66[] = {0xFF};
static const uint8_t lcd_cmd_data_67[] = {0x00};
static const uint8_t lcd_cmd_data_68[] = {0x00};
static const uint8_t lcd_cmd_data_69[] = {0x30};
static const uint8_t lcd_cmd_data_70[] = {0x00};
static const uint8_t lcd_cmd_data_71[] = {0x00};
static const uint8_t lcd_cmd_data_72[] = {0x00};
static const uint8_t lcd_cmd_data_73[] = {0x00};
static const uint8_t lcd_cmd_data_74[] = {0x42};
static const uint8_t lcd_cmd_data_75[] = {0xE0};
static const uint8_t lcd_cmd_data_76[] = {0x40};
static const uint8_t lcd_cmd_data_77[] = {0x40};
static const uint8_t lcd_cmd_data_78[] = {0x02};
static const uint8_t lcd_cmd_data_79[] = {0x00};
static const uint8_t lcd_cmd_data_80[] = {0x40};
static const uint8_t lcd_cmd_data_81[] = {0x03};
static const uint8_t lcd_cmd_data_82[] = {0x00};
static const uint8_t lcd_cmd_data_83[] = {0x00};
static const uint8_t lcd_cmd_data_84[] = {0x00};
static const uint8_t lcd_cmd_data_85[] = {0x00};
static const uint8_t lcd_cmd_data_86[] = {0x42};
static const uint8_t lcd_cmd_data_87[] = {0xE0};
static const uint8_t lcd_cmd_data_88[] = {0x40};
static const uint8_t lcd_cmd_data_89[] = {0x40};
static const uint8_t lcd_cmd_data_90[] = {0x02};
static const uint8_t lcd_cmd_data_91[] = {0x00};
static const uint8_t lcd_cmd_data_92[] = {0x40};
static const uint8_t lcd_cmd_data_93[] = {0x03};
static const uint8_t lcd_cmd_data_94[] = {0x00};
static const uint8_t lcd_cmd_data_95[] = {0x00};
static const uint8_t lcd_cmd_data_96[] = {0x00};
static const uint8_t lcd_cmd_data_97[] = {0x00};
static const uint8_t lcd_cmd_data_98[] = {0x38};
static const uint8_t lcd_cmd_data_99[] = {0x38};
static const uint8_t lcd_cmd_data_100[] = {0x00};
static const uint8_t lcd_cmd_data_101[] = {0x04};
static const uint8_t lcd_cmd_data_102[] = {0x04};
static const uint8_t lcd_cmd_data_103[] = {0x02};
static const uint8_t lcd_cmd_data_104[] = {0xDC};
static const uint8_t lcd_cmd_data_105[] = {0xDC};
static const uint8_t lcd_cmd_data_106[] = {0x00};
static const uint8_t lcd_cmd_data_107[] = {0x00};
static const uint8_t lcd_cmd_data_108[] = {0x00};
static const uint8_t lcd_cmd_data_109[] = {0x38};
static const uint8_t lcd_cmd_data_110[] = {0x38};
static const uint8_t lcd_cmd_data_111[] = {0x00};
static const uint8_t lcd_cmd_data_112[] = {0x06};
static const uint8_t lcd_cmd_data_113[] = {0x06};
static const uint8_t lcd_cmd_data_114[] = {0x02};
static const uint8_t lcd_cmd_data_115[] = {0xDE};
static const uint8_t lcd_cmd_data_116[] = {0xDE};
static const uint8_t lcd_cmd_data_117[] = {0x00};
static const uint8_t lcd_cmd_data_118[] = {0x00};
static const uint8_t lcd_cmd_data_119[] = {0x00};
static const uint8_t lcd_cmd_data_120[] = {0x38};
static const uint8_t lcd_cmd_data_121[] = {0x38};
static const uint8_t lcd_cmd_data_122[] = {0x00};
static const uint8_t lcd_cmd_data_123[] = {0x08};
static const uint8_t lcd_cmd_data_124[] = {0x08};
static const uint8_t lcd_cmd_data_125[] = {0x02};
static const uint8_t lcd_cmd_data_126[] = {0xE0};
static const uint8_t lcd_cmd_data_127[] = {0xE0};
static const uint8_t lcd_cmd_data_128[] = {0x00};
static const uint8_t lcd_cmd_data_129[] = {0x00};
static const uint8_t lcd_cmd_data_130[] = {0x00};
static const uint8_t lcd_cmd_data_131[] = {0x38};
static const uint8_t lcd_cmd_data_132[] = {0x38};
static const uint8_t lcd_cmd_data_133[] = {0x00};
static const uint8_t lcd_cmd_data_134[] = {0x0A};
static const uint8_t lcd_cmd_data_135[] = {0x0A};
static const uint8_t lcd_cmd_data_136[] = {0x02};
static const uint8_t lcd_cmd_data_137[] = {0xE2};
static const uint8_t lcd_cmd_data_138[] = {0xE2};
static const uint8_t lcd_cmd_data_139[] = {0x00};
static const uint8_t lcd_cmd_data_140[] = {0x00};
static const uint8_t lcd_cmd_data_141[] = {0x00};
static const uint8_t lcd_cmd_data_142[] = {0x38};
static const uint8_t lcd_cmd_data_143[] = {0x38};
static const uint8_t lcd_cmd_data_144[] = {0x00};
static const uint8_t lcd_cmd_data_145[] = {0x03};
static const uint8_t lcd_cmd_data_146[] = {0x03};
static const uint8_t lcd_cmd_data_147[] = {0x02};
static const uint8_t lcd_cmd_data_148[] = {0xDB};
static const uint8_t lcd_cmd_data_149[] = {0xDB};
static const uint8_t lcd_cmd_data_150[] = {0x00};
static const uint8_t lcd_cmd_data_151[] = {0x00};
static const uint8_t lcd_cmd_data_152[] = {0x00};
static const uint8_t lcd_cmd_data_153[] = {0x38};
static const uint8_t lcd_cmd_data_154[] = {0x38};
static const uint8_t lcd_cmd_data_155[] = {0x00};
static const uint8_t lcd_cmd_data_156[] = {0x05};
static const uint8_t lcd_cmd_data_157[] = {0x05};
static const uint8_t lcd_cmd_data_158[] = {0x02};
static const uint8_t lcd_cmd_data_159[] = {0xDD};
static const uint8_t lcd_cmd_data_160[] = {0xDD};
static const uint8_t lcd_cmd_data_161[] = {0x00};
static const uint8_t lcd_cmd_data_162[] = {0x00};
static const uint8_t lcd_cmd_data_163[] = {0x00};
static const uint8_t lcd_cmd_data_164[] = {0x38};
static const uint8_t lcd_cmd_data_165[] = {0x38};
static const uint8_t lcd_cmd_data_166[] = {0x00};
static const uint8_t lcd_cmd_data_167[] = {0x07};
static const uint8_t lcd_cmd_data_168[] = {0x07};
static const uint8_t lcd_cmd_data_169[] = {0x02};
static const uint8_t lcd_cmd_data_170[] = {0xDF};
static const uint8_t lcd_cmd_data_171[] = {0xDF};
static const uint8_t lcd_cmd_data_172[] = {0x00};
static const uint8_t lcd_cmd_data_173[] = {0x00};
static const uint8_t lcd_cmd_data_174[] = {0x00};
static const uint8_t lcd_cmd_data_175[] = {0x38};
static const uint8_t lcd_cmd_data_176[] = {0x38};
static const uint8_t lcd_cmd_data_177[] = {0x00};
static const uint8_t lcd_cmd_data_178[] = {0x09};
static const uint8_t lcd_cmd_data_179[] = {0x09};
static const uint8_t lcd_cmd_data_180[] = {0x02};
static const uint8_t lcd_cmd_data_181[] = {0xE1};
static const uint8_t lcd_cmd_data_182[] = {0xE1};
static const uint8_t lcd_cmd_data_183[] = {0x00};
static const uint8_t lcd_cmd_data_184[] = {0x00};
static const uint8_t lcd_cmd_data_185[] = {0x00};
static const uint8_t lcd_cmd_data_186[] = {0x22};
static const uint8_t lcd_cmd_data_187[] = {0x22};
static const uint8_t lcd_cmd_data_188[] = {0xAA};
static const uint8_t lcd_cmd_data_189[] = {0x65};
static const uint8_t lcd_cmd_data_190[] = {0x74};
static const uint8_t lcd_cmd_data_191[] = {0x47};
static const uint8_t lcd_cmd_data_192[] = {0x56};
static const uint8_t lcd_cmd_data_193[] = {0x00};
static const uint8_t lcd_cmd_data_194[] = {0x88};
static const uint8_t lcd_cmd_data_195[] = {0x99};
static const uint8_t lcd_cmd_data_196[] = {0x33};
static const uint8_t lcd_cmd_data_197[] = {0x11};
static const uint8_t lcd_cmd_data_198[] = {0x11};
static const uint8_t lcd_cmd_data_199[] = {0xAA};
static const uint8_t lcd_cmd_data_200[] = {0x65};
static const uint8_t lcd_cmd_data_201[] = {0x74};
static const uint8_t lcd_cmd_data_202[] = {0x47};
static const uint8_t lcd_cmd_data_203[] = {0x56};
static const uint8_t lcd_cmd_data_204[] = {0x00};
static const uint8_t lcd_cmd_data_205[] = {0x88};
static const uint8_t lcd_cmd_data_206[] = {0x99};
static const uint8_t lcd_cmd_data_207[] = {0x33};
static const uint8_t lcd_cmd_data_208[] = {0x01};
static const uint8_t lcd_cmd_data_209[] = {0x00};
static const uint8_t lcd_cmd_data_210[] = {0x55};
static const uint8_t lcd_cmd_data_211[] = {0x00};
static const uint8_t lcd_cmd_data_212[] = {0x00};
static const uint8_t lcd_cmd_data_213[] = {0x00};
static const uint8_t lcd_cmd_data_214[] = {0x00};
static const uint8_t lcd_cmd_data_215[] = {0x28};
static const uint8_t lcd_cmd_data_216[] = {0x28};
static const uint8_t lcd_cmd_data_217[] = {0xF0};
static const uint8_t lcd_cmd_data_218[] = {0xD1};
static const uint8_t lcd_cmd_data_219[] = {0xE0};
static const uint8_t lcd_cmd_data_220[] = {0x61};
static const uint8_t lcd_cmd_data_221[] = {0x82};
static const uint8_t lcd_cmd_data_222[] = {0x00};
static const uint8_t lcd_cmd_data_223[] = {0x01};
static const uint8_t lcd_cmd_data_224[] = {0x01};
static const uint8_t lcd_cmd_data_225[] = {0x56};
static const uint8_t lcd_cmd_data_226[] = {0x4D};
static const uint8_t lcd_cmd_data_227[] = {0x24};
static const uint8_t lcd_cmd_data_228[] = {0x87};
static const uint8_t lcd_cmd_data_229[] = {0x44};
static const uint8_t lcd_cmd_data_230[] = {0x8B};
static const uint8_t lcd_cmd_data_231[] = {0x40};
static const uint8_t lcd_cmd_data_232[] = {0x86};
static const uint8_t lcd_cmd_data_233[] = {0x00};
static const uint8_t lcd_cmd_data_234[] = {0x08};
static const uint8_t lcd_cmd_data_235[] = {0x08};
static const uint8_t lcd_cmd_data_236[] = {0x00};
static const uint8_t lcd_cmd_data_237[] = {0x80};
static const uint8_t lcd_cmd_data_238[] = {0x10};
static const uint8_t lcd_cmd_data_239[] = {0x37};
static const uint8_t lcd_cmd_data_240[] = {0x80};
static const uint8_t lcd_cmd_data_241[] = {0x10};
static const uint8_t lcd_cmd_data_242[] = {0x37};
static const uint8_t lcd_cmd_data_243[] = {0xA9};
static const uint8_t lcd_cmd_data_244[] = {0x41};
static const uint8_t lcd_cmd_data_245[] = {0x01};
static const uint8_t lcd_cmd_data_246[] = {0xA9};
static const uint8_t lcd_cmd_data_247[] = {0x41};
static const uint8_t lcd_cmd_data_248[] = {0x01};
static const uint8_t lcd_cmd_data_249[] = {0x91};
static const uint8_t lcd_cmd_data_250[] = {0x68};
static const uint8_t lcd_cmd_data_251[] = {0x68};
static const uint8_t lcd_cmd_data_252[] = {0x00, 0xA5};
static const uint8_t lcd_cmd_data_253[] = {0x4F};
static const uint8_t lcd_cmd_data_254[] = {0x4F};
static const uint8_t lcd_cmd_data_255[] = {0x10};
static const uint8_t lcd_cmd_data_256[] = {0x00};
static const uint8_t lcd_cmd_data_257[] = {0x02};
static const uint8_t lcd_cmd_data_258[] = {0xF0, 0x0A, 0x10, 0x09, 0x09, 0x36, 0x35, 0x33, 0x4A, 0x29, 0x15, 0x15, 0x2E, 0x34};
static const uint8_t lcd_cmd_data_259[] = {0xF0, 0x0A, 0x0F, 0x08, 0x08, 0x05, 0x34, 0x33, 0x4A, 0x39, 0x15, 0x15, 0x2D, 0x33};
static const uint8_t lcd_cmd_data_260[] = {0x10};
static const uint8_t lcd_cmd_data_261[] = {0x10};
static const uint8_t lcd_cmd_data_262[] = {0x07};
static const uint8_t lcd_cmd_data_263[] = {0x00};
static const uint8_t lcd_cmd_data_264[] = {0x00};
static const uint8_t lcd_cmd_data_265[] = {0x00};
static const uint8_t lcd_cmd_data_266[] = {0xE0};
static const uint8_t lcd_cmd_data_267[] = {0x06};
static const uint8_t lcd_cmd_data_268[] = {0x21};
static const uint8_t lcd_cmd_data_269[] = {0x01};
static const uint8_t lcd_cmd_data_270[] = {0x05};
static const uint8_t lcd_cmd_data_271[] = {0x02};
static const uint8_t lcd_cmd_data_272[] = {0xDA};
static const uint8_t lcd_cmd_data_273[] = {0x00};
static const uint8_t lcd_cmd_data_274[] = {0x00};
static const uint8_t lcd_cmd_data_275[] = {0x0F};
static const uint8_t lcd_cmd_data_276[] = {0x00};
static const uint8_t lcd_cmd_data_277[] = {0x00};
static const uint8_t lcd_cmd_data_278[] = {0x00};
static const uint8_t lcd_cmd_data_279[] = {0x00};
static const uint8_t lcd_cmd_data_280[] = {0x00};
static const uint8_t lcd_cmd_data_281[] = {0x00};
static const uint8_t lcd_cmd_data_282[] = {0x00};
static const uint8_t lcd_cmd_data_283[] = {0x00};
static const uint8_t lcd_cmd_data_284[] = {0x00};
static const uint8_t lcd_cmd_data_285[] = {0x00};
static const uint8_t lcd_cmd_data_286[] = {0x40};
static const uint8_t lcd_cmd_data_287[] = {0x04};
static const uint8_t lcd_cmd_data_288[] = {0x00};
static const uint8_t lcd_cmd_data_289[] = {0x42};
static const uint8_t lcd_cmd_data_290[] = {0xD9};
static const uint8_t lcd_cmd_data_291[] = {0x00};
static const uint8_t lcd_cmd_data_292[] = {0x00};
static const uint8_t lcd_cmd_data_293[] = {0x00};
static const uint8_t lcd_cmd_data_294[] = {0x00};
static const uint8_t lcd_cmd_data_295[] = {0x00};
static const uint8_t lcd_cmd_data_296[] = {0x00};
static const uint8_t lcd_cmd_data_297[] = {0x00};
static const uint8_t lcd_cmd_data_298[] = {0x40};
static const uint8_t lcd_cmd_data_299[] = {0x03};
static const uint8_t lcd_cmd_data_300[] = {0x00};
static const uint8_t lcd_cmd_data_301[] = {0x42};
static const uint8_t lcd_cmd_data_302[] = {0xD8};
static const uint8_t lcd_cmd_data_303[] = {0x00};
static const uint8_t lcd_cmd_data_304[] = {0x00};
static const uint8_t lcd_cmd_data_305[] = {0x00};
static const uint8_t lcd_cmd_data_306[] = {0x00};
static const uint8_t lcd_cmd_data_307[] = {0x00};
static const uint8_t lcd_cmd_data_308[] = {0x00};
static const uint8_t lcd_cmd_data_309[] = {0x00};
static const uint8_t lcd_cmd_data_310[] = {0x48};
static const uint8_t lcd_cmd_data_311[] = {0x00};
static const uint8_t lcd_cmd_data_312[] = {0x06};
static const uint8_t lcd_cmd_data_313[] = {0x02};
static const uint8_t lcd_cmd_data_314[] = {0xD6};
static const uint8_t lcd_cmd_data_315[] = {0x04};
static const uint8_t lcd_cmd_data_316[] = {0x00};
static const uint8_t lcd_cmd_data_317[] = {0x00};
static const uint8_t lcd_cmd_data_318[] = {0x48};
static const uint8_t lcd_cmd_data_319[] = {0x00};
static const uint8_t lcd_cmd_data_320[] = {0x08};
static const uint8_t lcd_cmd_data_321[] = {0x02};
static const uint8_t lcd_cmd_data_322[] = {0xD8};
static const uint8_t lcd_cmd_data_323[] = {0x04};
static const uint8_t lcd_cmd_data_324[] = {0x00};
static const uint8_t lcd_cmd_data_325[] = {0x00};
static const uint8_t lcd_cmd_data_326[] = {0x48};
static const uint8_t lcd_cmd_data_327[] = {0x00};
static const uint8_t lcd_cmd_data_328[] = {0x0A};
static const uint8_t lcd_cmd_data_329[] = {0x02};
static const uint8_t lcd_cmd_data_330[] = {0xDA};
static const uint8_t lcd_cmd_data_331[] = {0x04};
static const uint8_t lcd_cmd_data_332[] = {0x00};
static const uint8_t lcd_cmd_data_333[] = {0x00};
static const uint8_t lcd_cmd_data_334[] = {0x48};
static const uint8_t lcd_cmd_data_335[] = {0x00};
static const uint8_t lcd_cmd_data_336[] = {0x0C};
static const uint8_t lcd_cmd_data_337[] = {0x02};
static const uint8_t lcd_cmd_data_338[] = {0xDC};
static const uint8_t lcd_cmd_data_339[] = {0x04};
static const uint8_t lcd_cmd_data_340[] = {0x00};
static const uint8_t lcd_cmd_data_341[] = {0x00};
static const uint8_t lcd_cmd_data_342[] = {0x48};
static const uint8_t lcd_cmd_data_343[] = {0x00};
static const uint8_t lcd_cmd_data_344[] = {0x05};
static const uint8_t lcd_cmd_data_345[] = {0x02};
static const uint8_t lcd_cmd_data_346[] = {0xD5};
static const uint8_t lcd_cmd_data_347[] = {0x04};
static const uint8_t lcd_cmd_data_348[] = {0x00};
static const uint8_t lcd_cmd_data_349[] = {0x00};
static const uint8_t lcd_cmd_data_350[] = {0x48};
static const uint8_t lcd_cmd_data_351[] = {0x00};
static const uint8_t lcd_cmd_data_352[] = {0x07};
static const uint8_t lcd_cmd_data_353[] = {0x02};
static const uint8_t lcd_cmd_data_354[] = {0xD7};
static const uint8_t lcd_cmd_data_355[] = {0x04};
static const uint8_t lcd_cmd_data_356[] = {0x00};
static const uint8_t lcd_cmd_data_357[] = {0x00};
static const uint8_t lcd_cmd_data_358[] = {0x48};
static const uint8_t lcd_cmd_data_359[] = {0x00};
static const uint8_t lcd_cmd_data_360[] = {0x09};
static const uint8_t lcd_cmd_data_361[] = {0x02};
static const uint8_t lcd_cmd_data_362[] = {0xD9};
static const uint8_t lcd_cmd_data_363[] = {0x04};
static const uint8_t lcd_cmd_data_364[] = {0x00};
static const uint8_t lcd_cmd_data_365[] = {0x00};
static const uint8_t lcd_cmd_data_366[] = {0x48};
static const uint8_t lcd_cmd_data_367[] = {0x00};
static const uint8_t lcd_cmd_data_368[] = {0x0B};
static const uint8_t lcd_cmd_data_369[] = {0x02};
static const uint8_t lcd_cmd_data_370[] = {0xDB};
static const uint8_t lcd_cmd_data_371[] = {0x04};
static const uint8_t lcd_cmd_data_372[] = {0x00};
static const uint8_t lcd_cmd_data_373[] = {0x00};
static const uint8_t lcd_cmd_data_374[] = {0x10};
static const uint8_t lcd_cmd_data_375[] = {0x47};
static const uint8_t lcd_cmd_data_376[] = {0x56};
static const uint8_t lcd_cmd_data_377[] = {0x65};
static const uint8_t lcd_cmd_data_378[] = {0x74};
static const uint8_t lcd_cmd_data_379[] = {0x88};
static const uint8_t lcd_cmd_data_380[] = {0x99};
static const uint8_t lcd_cmd_data_381[] = {0x01};
static const uint8_t lcd_cmd_data_382[] = {0xBB};
static const uint8_t lcd_cmd_data_383[] = {0xAA};
static const uint8_t lcd_cmd_data_384[] = {0x10};
static const uint8_t lcd_cmd_data_385[] = {0x47};
static const uint8_t lcd_cmd_data_386[] = {0x56};
static const uint8_t lcd_cmd_data_387[] = {0x65};
static const uint8_t lcd_cmd_data_388[] = {0x74};
static const uint8_t lcd_cmd_data_389[] = {0x88};
static const uint8_t lcd_cmd_data_390[] = {0x99};
static const uint8_t lcd_cmd_data_391[] = {0x01};
static const uint8_t lcd_cmd_data_392[] = {0xBB};
static const uint8_t lcd_cmd_data_393[] = {0xAA};
static const uint8_t lcd_cmd_data_394[] = {0x01};
static const uint8_t lcd_cmd_data_395[] = {0x00};
static const uint8_t lcd_cmd_data_396[] = {0x00};
static const uint8_t lcd_cmd_data_397[] = {0x00};
static const uint8_t lcd_cmd_data_398[] = {0x00};
static const uint8_t lcd_cmd_data_399[] = {0x00};

static const st77916_lcd_init_cmd_t vendor_specific_init_version_1[] = {
    {0xF0, lcd_cmd_data_0, 1, 0},
    {0xF2, lcd_cmd_data_1, 1, 0},
    {0x7C, lcd_cmd_data_2, 1, 0},
    {0x83, lcd_cmd_data_3, 1, 0},
    {0x84, lcd_cmd_data_4, 1, 0},
    {0xF2, lcd_cmd_data_5, 1, 0},
    {0xF0, lcd_cmd_data_6, 1, 0},
    {0xF0, lcd_cmd_data_7, 1, 0},
    {0xF1, lcd_cmd_data_8, 1, 0},
    {0xB0, lcd_cmd_data_9, 1, 0},
    {0xB1, lcd_cmd_data_10, 1, 0},
    {0xB2, lcd_cmd_data_11, 1, 0},
    {0xB4, lcd_cmd_data_12, 1, 0},
    {0xB5, lcd_cmd_data_13, 1, 0},
    {0xB6, lcd_cmd_data_14, 1, 0},
    {0xB7, lcd_cmd_data_15, 1, 0},
    {0xB8, lcd_cmd_data_16, 1, 0},
    {0xBA, lcd_cmd_data_17, 1, 0},
    {0xBB, lcd_cmd_data_18, 1, 0},
    {0xBC, lcd_cmd_data_19, 1, 0},
    {0xBD, lcd_cmd_data_20, 1, 0},
    {0xC0, lcd_cmd_data_21, 1, 0},
    {0xC1, lcd_cmd_data_22, 1, 0},
    {0xC2, lcd_cmd_data_23, 1, 0},
    {0xC3, lcd_cmd_data_24, 1, 0},
    {0xC4, lcd_cmd_data_25, 1, 0},
    {0xC5, lcd_cmd_data_26, 1, 0},
    {0xC6, lcd_cmd_data_27, 1, 0},
    {0xC7, lcd_cmd_data_28, 1, 0},
    {0xC8, lcd_cmd_data_29, 1, 0},
    {0xC9, lcd_cmd_data_30, 1, 0},
    {0xCA, lcd_cmd_data_31, 1, 0},
    {0xCB, lcd_cmd_data_32, 1, 0},
    {0xD0, lcd_cmd_data_33, 1, 0},
    {0xD1, lcd_cmd_data_34, 1, 0},
    {0xD2, lcd_cmd_data_35, 1, 0},
    {0xF5, lcd_cmd_data_36, 2, 0},
    // {0xDD, lcd_cmd_data_37, 1, 0},
    // {0xDE, lcd_cmd_data_38, 1, 0},
    // {0xDD, lcd_cmd_data_39, 1, 0},
    // {0xDE, lcd_cmd_data_40, 1, 0},
    {0xF1, lcd_cmd_data_41, 1, 0},
    {0xF0, lcd_cmd_data_42, 1, 0},
    {0xF0, lcd_cmd_data_43, 1, 0},
    {0xE0, lcd_cmd_data_44, 14, 0},
    {0xE1, lcd_cmd_data_45, 14, 0},
    // {0xE0, lcd_cmd_data_46, 14, 0},
    // {0xE1, lcd_cmd_data_47, 14, 0},
    {0xF0, lcd_cmd_data_48, 1, 0},
    {0xF3, lcd_cmd_data_49, 1, 0},
    {0xE0, lcd_cmd_data_50, 1, 0},
    {0xE1, lcd_cmd_data_51, 1, 0},
    {0xE2, lcd_cmd_data_52, 1, 0},
    {0xE3, lcd_cmd_data_53, 1, 0},
    {0xE4, lcd_cmd_data_54, 1, 0},
    {0xE5, lcd_cmd_data_55, 1, 0},
    {0xE6, lcd_cmd_data_56, 1, 0},
    {0xE7, lcd_cmd_data_57, 1, 0},
    {0xE8, lcd_cmd_data_58, 1, 0},
    {0xE9, lcd_cmd_data_59, 1, 0},
    {0xEA, lcd_cmd_data_60, 1, 0},
    {0xEB, lcd_cmd_data_61, 1, 0},
    {0xEC, lcd_cmd_data_62, 1, 0},
    {0xED, lcd_cmd_data_63, 1, 0},
    {0xEE, lcd_cmd_data_64, 1, 0},
    {0xEF, lcd_cmd_data_65, 1, 0},
    {0xF8, lcd_cmd_data_66, 1, 0},
    {0xF9, lcd_cmd_data_67, 1, 0},
    {0xFA, lcd_cmd_data_68, 1, 0},
    {0xFB, lcd_cmd_data_69, 1, 0},
    {0xFC, lcd_cmd_data_70, 1, 0},
    {0xFD, lcd_cmd_data_71, 1, 0},
    {0xFE, lcd_cmd_data_72, 1, 0},
    {0xFF, lcd_cmd_data_73, 1, 0},
    {0x60, lcd_cmd_data_74, 1, 0},
    {0x61, lcd_cmd_data_75, 1, 0},
    {0x62, lcd_cmd_data_76, 1, 0},
    {0x63, lcd_cmd_data_77, 1, 0},
    {0x64, lcd_cmd_data_78, 1, 0},
    {0x65, lcd_cmd_data_79, 1, 0},
    {0x66, lcd_cmd_data_80, 1, 0},
    {0x67, lcd_cmd_data_81, 1, 0},
    {0x68, lcd_cmd_data_82, 1, 0},
    {0x69, lcd_cmd_data_83, 1, 0},
    {0x6A, lcd_cmd_data_84, 1, 0},
    {0x6B, lcd_cmd_data_85, 1, 0},
    {0x70, lcd_cmd_data_86, 1, 0},
    {0x71, lcd_cmd_data_87, 1, 0},
    {0x72, lcd_cmd_data_88, 1, 0},
    {0x73, lcd_cmd_data_89, 1, 0},
    {0x74, lcd_cmd_data_90, 1, 0},
    {0x75, lcd_cmd_data_91, 1, 0},
    {0x76, lcd_cmd_data_92, 1, 0},
    {0x77, lcd_cmd_data_93, 1, 0},
    {0x78, lcd_cmd_data_94, 1, 0},
    {0x79, lcd_cmd_data_95, 1, 0},
    {0x7A, lcd_cmd_data_96, 1, 0},
    {0x7B, lcd_cmd_data_97, 1, 0},
    // {0x80, lcd_cmd_data_98, 1, 0},
    {0x80, lcd_cmd_data_99, 1, 0},
    {0x81, lcd_cmd_data_100, 1, 0},
    // {0x82, lcd_cmd_data_101, 1, 0},
    {0x82, lcd_cmd_data_102, 1, 0},
    {0x83, lcd_cmd_data_103, 1, 0},
    // {0x84, lcd_cmd_data_104, 1, 0},
    {0x84, lcd_cmd_data_105, 1, 0},
    {0x85, lcd_cmd_data_106, 1, 0},
    {0x86, lcd_cmd_data_107, 1, 0},
    {0x87, lcd_cmd_data_108, 1, 0},
    // {0x88, lcd_cmd_data_109, 1, 0},
    {0x88, lcd_cmd_data_110, 1, 0},
    {0x89, lcd_cmd_data_111, 1, 0},
    // {0x8A, lcd_cmd_data_112, 1, 0},
    {0x8A, lcd_cmd_data_113, 1, 0},
    {0x8B, lcd_cmd_data_114, 1, 0},
    // {0x8C, lcd_cmd_data_115, 1, 0},
    {0x8C, lcd_cmd_data_116, 1, 0},
    {0x8D, lcd_cmd_data_117, 1, 0},
    {0x8E, lcd_cmd_data_118, 1, 0},
    {0x8F, lcd_cmd_data_119, 1, 0},
    // {0x90, lcd_cmd_data_120, 1, 0},
    {0x90, lcd_cmd_data_121, 1, 0},
    {0x91, lcd_cmd_data_122, 1, 0},
    // {0x92, lcd_cmd_data_123, 1, 0},
    {0x92, lcd_cmd_data_124, 1, 0},
    {0x93, lcd_cmd_data_125, 1, 0},
    // {0x94, lcd_cmd_data_126, 1, 0},
    {0x94, lcd_cmd_data_127, 1, 0},
    {0x95, lcd_cmd_data_128, 1, 0},
    {0x96, lcd_cmd_data_129, 1, 0},
    {0x97, lcd_cmd_data_130, 1, 0},
    // {0x98, lcd_cmd_data_131, 1, 0},
    {0x98, lcd_cmd_data_132, 1, 0},
    {0x99, lcd_cmd_data_133, 1, 0},
    // {0x9A, lcd_cmd_data_134, 1, 0},
    {0x9A, lcd_cmd_data_135, 1, 0},
    {0x9B, lcd_cmd_data_136, 1, 0},
    // {0x9C, lcd_cmd_data_137, 1, 0},
    {0x9C, lcd_cmd_data_138, 1, 0},
    {0x9D, lcd_cmd_data_139, 1, 0},
    {0x9E, lcd_cmd_data_140, 1, 0},
    {0x9F, lcd_cmd_data_141, 1, 0},
    // {0xA0, lcd_cmd_data_142, 1, 0},
    {0xA0, lcd_cmd_data_143, 1, 0},
    {0xA1, lcd_cmd_data_144, 1, 0},
    // {0xA2, lcd_cmd_data_145, 1, 0},
    {0xA2, lcd_cmd_data_146, 1, 0},
    {0xA3, lcd_cmd_data_147, 1, 0},
    // {0xA4, lcd_cmd_data_148, 1, 0},
    {0xA4, lcd_cmd_data_149, 1, 0},
    {0xA5, lcd_cmd_data_150, 1, 0},
    {0xA6, lcd_cmd_data_151, 1, 0},
    {0xA7, lcd_cmd_data_152, 1, 0},
    // {0xA8, lcd_cmd_data_153, 1, 0},
    {0xA8, lcd_cmd_data_154, 1, 0},
    {0xA9, lcd_cmd_data_155, 1, 0},
    // {0xAA, lcd_cmd_data_156, 1, 0},
    {0xAA, lcd_cmd_data_157, 1, 0},
    {0xAB, lcd_cmd_data_158, 1, 0},
    // {0xAC, lcd_cmd_data_159, 1, 0},
    {0xAC, lcd_cmd_data_160, 1, 0},
    {0xAD, lcd_cmd_data_161, 1, 0},
    {0xAE, lcd_cmd_data_162, 1, 0},
    {0xAF, lcd_cmd_data_163, 1, 0},
    // {0xB0, lcd_cmd_data_164, 1, 0},
    {0xB0, lcd_cmd_data_165, 1, 0},
    {0xB1, lcd_cmd_data_166, 1, 0},
    // {0xB2, lcd_cmd_data_167, 1, 0},
    {0xB2, lcd_cmd_data_168, 1, 0},
    {0xB3, lcd_cmd_data_169, 1, 0},
    // {0xB4, lcd_cmd_data_170, 1, 0},
    {0xB4, lcd_cmd_data_171, 1, 0},
    {0xB5, lcd_cmd_data_172, 1, 0},
    {0xB6, lcd_cmd_data_173, 1, 0},
    {0xB7, lcd_cmd_data_174, 1, 0},
    // {0xB8, lcd_cmd_data_175, 1, 0},
    {0xB8, lcd_cmd_data_176, 1, 0},
    {0xB9, lcd_cmd_data_177, 1, 0},
    // {0xBA, lcd_cmd_data_178, 1, 0},
    {0xBA, lcd_cmd_data_179, 1, 0},
    {0xBB, lcd_cmd_data_180, 1, 0},
    // {0xBC, lcd_cmd_data_181, 1, 0},
    {0xBC, lcd_cmd_data_182, 1, 0},
    {0xBD, lcd_cmd_data_183, 1, 0},
    {0xBE, lcd_cmd_data_184, 1, 0},
    {0xBF, lcd_cmd_data_185, 1, 0},
    // {0xC0, lcd_cmd_data_186, 1, 0},
    {0xC0, lcd_cmd_data_187, 1, 0},
    {0xC1, lcd_cmd_data_188, 1, 0},
    {0xC2, lcd_cmd_data_189, 1, 0},
    {0xC3, lcd_cmd_data_190, 1, 0},
    {0xC4, lcd_cmd_data_191, 1, 0},
    {0xC5, lcd_cmd_data_192, 1, 0},
    {0xC6, lcd_cmd_data_193, 1, 0},
    {0xC7, lcd_cmd_data_194, 1, 0},
    {0xC8, lcd_cmd_data_195, 1, 0},
    {0xC9, lcd_cmd_data_196, 1, 0},
    // {0xD0, lcd_cmd_data_197, 1, 0},
    {0xD0, lcd_cmd_data_198, 1, 0},
    {0xD1, lcd_cmd_data_199, 1, 0},
    {0xD2, lcd_cmd_data_200, 1, 0},
    {0xD3, lcd_cmd_data_201, 1, 0},
    {0xD4, lcd_cmd_data_202, 1, 0},
    {0xD5, lcd_cmd_data_203, 1, 0},
    {0xD6, lcd_cmd_data_204, 1, 0},
    {0xD7, lcd_cmd_data_205, 1, 0},
    {0xD8, lcd_cmd_data_206, 1, 0},
    {0xD9, lcd_cmd_data_207, 1, 0},
    {0xF3, lcd_cmd_data_208, 1, 0},
    {0xF0, lcd_cmd_data_209, 1, 0},
    // {0x3A, lcd_cmd_data_210, 1, 0},
    {0x35, lcd_cmd_data_211, 1, 0},
    {0x21, lcd_cmd_data_212, 0, 0},
    {0x11, lcd_cmd_data_213, 0, 120},
    {0x29, lcd_cmd_data_214, 0, 0},
};
static const st77916_lcd_init_cmd_t vendor_specific_init_version_2[] = {
  {0xF0, lcd_cmd_data_215, 1, 0},
  {0xF2, lcd_cmd_data_216, 1, 0},
  {0x73, lcd_cmd_data_217, 1, 0},
  {0x7C, lcd_cmd_data_218, 1, 0},
  {0x83, lcd_cmd_data_219, 1, 0},
  {0x84, lcd_cmd_data_220, 1, 0},
  {0xF2, lcd_cmd_data_221, 1, 0},
  {0xF0, lcd_cmd_data_222, 1, 0},
  {0xF0, lcd_cmd_data_223, 1, 0},
  {0xF1, lcd_cmd_data_224, 1, 0},
  {0xB0, lcd_cmd_data_225, 1, 0},
  {0xB1, lcd_cmd_data_226, 1, 0},
  {0xB2, lcd_cmd_data_227, 1, 0},
  {0xB4, lcd_cmd_data_228, 1, 0},
  {0xB5, lcd_cmd_data_229, 1, 0},
  {0xB6, lcd_cmd_data_230, 1, 0},
  {0xB7, lcd_cmd_data_231, 1, 0},
  {0xB8, lcd_cmd_data_232, 1, 0},
  {0xBA, lcd_cmd_data_233, 1, 0},
  {0xBB, lcd_cmd_data_234, 1, 0},
  {0xBC, lcd_cmd_data_235, 1, 0},
  {0xBD, lcd_cmd_data_236, 1, 0},
  {0xC0, lcd_cmd_data_237, 1, 0},
  {0xC1, lcd_cmd_data_238, 1, 0},
  {0xC2, lcd_cmd_data_239, 1, 0},
  {0xC3, lcd_cmd_data_240, 1, 0},
  {0xC4, lcd_cmd_data_241, 1, 0},
  {0xC5, lcd_cmd_data_242, 1, 0},
  {0xC6, lcd_cmd_data_243, 1, 0},
  {0xC7, lcd_cmd_data_244, 1, 0},
  {0xC8, lcd_cmd_data_245, 1, 0},
  {0xC9, lcd_cmd_data_246, 1, 0},
  {0xCA, lcd_cmd_data_247, 1, 0},
  {0xCB, lcd_cmd_data_248, 1, 0},
  {0xD0, lcd_cmd_data_249, 1, 0},
  {0xD1, lcd_cmd_data_250, 1, 0},
  {0xD2, lcd_cmd_data_251, 1, 0},
  {0xF5, lcd_cmd_data_252, 2, 0},
  {0xDD, lcd_cmd_data_253, 1, 0},
  {0xDE, lcd_cmd_data_254, 1, 0},
  {0xF1, lcd_cmd_data_255, 1, 0},
  {0xF0, lcd_cmd_data_256, 1, 0},
  {0xF0, lcd_cmd_data_257, 1, 0},
  {0xE0, lcd_cmd_data_258, 14, 0},
  {0xE1, lcd_cmd_data_259, 14, 0},
  {0xF0, lcd_cmd_data_260, 1, 0},
  {0xF3, lcd_cmd_data_261, 1, 0},
  {0xE0, lcd_cmd_data_262, 1, 0},
  {0xE1, lcd_cmd_data_263, 1, 0},
  {0xE2, lcd_cmd_data_264, 1, 0},
  {0xE3, lcd_cmd_data_265, 1, 0},
  {0xE4, lcd_cmd_data_266, 1, 0},
  {0xE5, lcd_cmd_data_267, 1, 0},
  {0xE6, lcd_cmd_data_268, 1, 0},
  {0xE7, lcd_cmd_data_269, 1, 0},
  {0xE8, lcd_cmd_data_270, 1, 0},
  {0xE9, lcd_cmd_data_271, 1, 0},
  {0xEA, lcd_cmd_data_272, 1, 0},
  {0xEB, lcd_cmd_data_273, 1, 0},
  {0xEC, lcd_cmd_data_274, 1, 0},
  {0xED, lcd_cmd_data_275, 1, 0},
  {0xEE, lcd_cmd_data_276, 1, 0},
  {0xEF, lcd_cmd_data_277, 1, 0},
  {0xF8, lcd_cmd_data_278, 1, 0},
  {0xF9, lcd_cmd_data_279, 1, 0},
  {0xFA, lcd_cmd_data_280, 1, 0},
  {0xFB, lcd_cmd_data_281, 1, 0},
  {0xFC, lcd_cmd_data_282, 1, 0},
  {0xFD, lcd_cmd_data_283, 1, 0},
  {0xFE, lcd_cmd_data_284, 1, 0},
  {0xFF, lcd_cmd_data_285, 1, 0},
  {0x60, lcd_cmd_data_286, 1, 0},
  {0x61, lcd_cmd_data_287, 1, 0},
  {0x62, lcd_cmd_data_288, 1, 0},
  {0x63, lcd_cmd_data_289, 1, 0},
  {0x64, lcd_cmd_data_290, 1, 0},
  {0x65, lcd_cmd_data_291, 1, 0},
  {0x66, lcd_cmd_data_292, 1, 0},
  {0x67, lcd_cmd_data_293, 1, 0},
  {0x68, lcd_cmd_data_294, 1, 0},
  {0x69, lcd_cmd_data_295, 1, 0},
  {0x6A, lcd_cmd_data_296, 1, 0},
  {0x6B, lcd_cmd_data_297, 1, 0},
  {0x70, lcd_cmd_data_298, 1, 0},
  {0x71, lcd_cmd_data_299, 1, 0},
  {0x72, lcd_cmd_data_300, 1, 0},
  {0x73, lcd_cmd_data_301, 1, 0},
  {0x74, lcd_cmd_data_302, 1, 0},
  {0x75, lcd_cmd_data_303, 1, 0},
  {0x76, lcd_cmd_data_304, 1, 0},
  {0x77, lcd_cmd_data_305, 1, 0},
  {0x78, lcd_cmd_data_306, 1, 0},
  {0x79, lcd_cmd_data_307, 1, 0},
  {0x7A, lcd_cmd_data_308, 1, 0},
  {0x7B, lcd_cmd_data_309, 1, 0},
  {0x80, lcd_cmd_data_310, 1, 0},
  {0x81, lcd_cmd_data_311, 1, 0},
  {0x82, lcd_cmd_data_312, 1, 0},
  {0x83, lcd_cmd_data_313, 1, 0},
  {0x84, lcd_cmd_data_314, 1, 0},
  {0x85, lcd_cmd_data_315, 1, 0},
  {0x86, lcd_cmd_data_316, 1, 0},
  {0x87, lcd_cmd_data_317, 1, 0},
  {0x88, lcd_cmd_data_318, 1, 0},
  {0x89, lcd_cmd_data_319, 1, 0},
  {0x8A, lcd_cmd_data_320, 1, 0},
  {0x8B, lcd_cmd_data_321, 1, 0},
  {0x8C, lcd_cmd_data_322, 1, 0},
  {0x8D, lcd_cmd_data_323, 1, 0},
  {0x8E, lcd_cmd_data_324, 1, 0},
  {0x8F, lcd_cmd_data_325, 1, 0},
  {0x90, lcd_cmd_data_326, 1, 0},
  {0x91, lcd_cmd_data_327, 1, 0},
  {0x92, lcd_cmd_data_328, 1, 0},
  {0x93, lcd_cmd_data_329, 1, 0},
  {0x94, lcd_cmd_data_330, 1, 0},
  {0x95, lcd_cmd_data_331, 1, 0},
  {0x96, lcd_cmd_data_332, 1, 0},
  {0x97, lcd_cmd_data_333, 1, 0},
  {0x98, lcd_cmd_data_334, 1, 0},
  {0x99, lcd_cmd_data_335, 1, 0},
  {0x9A, lcd_cmd_data_336, 1, 0},
  {0x9B, lcd_cmd_data_337, 1, 0},
  {0x9C, lcd_cmd_data_338, 1, 0},
  {0x9D, lcd_cmd_data_339, 1, 0},
  {0x9E, lcd_cmd_data_340, 1, 0},
  {0x9F, lcd_cmd_data_341, 1, 0},
  {0xA0, lcd_cmd_data_342, 1, 0},
  {0xA1, lcd_cmd_data_343, 1, 0},
  {0xA2, lcd_cmd_data_344, 1, 0},
  {0xA3, lcd_cmd_data_345, 1, 0},
  {0xA4, lcd_cmd_data_346, 1, 0},
  {0xA5, lcd_cmd_data_347, 1, 0},
  {0xA6, lcd_cmd_data_348, 1, 0},
  {0xA7, lcd_cmd_data_349, 1, 0},
  {0xA8, lcd_cmd_data_350, 1, 0},
  {0xA9, lcd_cmd_data_351, 1, 0},
  {0xAA, lcd_cmd_data_352, 1, 0},
  {0xAB, lcd_cmd_data_353, 1, 0},
  {0xAC, lcd_cmd_data_354, 1, 0},
  {0xAD, lcd_cmd_data_355, 1, 0},
  {0xAE, lcd_cmd_data_356, 1, 0},
  {0xAF, lcd_cmd_data_357, 1, 0},
  {0xB0, lcd_cmd_data_358, 1, 0},
  {0xB1, lcd_cmd_data_359, 1, 0},
  {0xB2, lcd_cmd_data_360, 1, 0},
  {0xB3, lcd_cmd_data_361, 1, 0},
  {0xB4, lcd_cmd_data_362, 1, 0},
  {0xB5, lcd_cmd_data_363, 1, 0},
  {0xB6, lcd_cmd_data_364, 1, 0},
  {0xB7, lcd_cmd_data_365, 1, 0},
  
  {0xB8, lcd_cmd_data_366, 1, 0},
  {0xB9, lcd_cmd_data_367, 1, 0},
  {0xBA, lcd_cmd_data_368, 1, 0},
  {0xBB, lcd_cmd_data_369, 1, 0},
  {0xBC, lcd_cmd_data_370, 1, 0},
  {0xBD, lcd_cmd_data_371, 1, 0},
  {0xBE, lcd_cmd_data_372, 1, 0},
  {0xBF, lcd_cmd_data_373, 1, 0},
  {0xC0, lcd_cmd_data_374, 1, 0},
  {0xC1, lcd_cmd_data_375, 1, 0},
  {0xC2, lcd_cmd_data_376, 1, 0},
  {0xC3, lcd_cmd_data_377, 1, 0},
  {0xC4, lcd_cmd_data_378, 1, 0},
  {0xC5, lcd_cmd_data_379, 1, 0},
  {0xC6, lcd_cmd_data_380, 1, 0},
  {0xC7, lcd_cmd_data_381, 1, 0},
  {0xC8, lcd_cmd_data_382, 1, 0},
  {0xC9, lcd_cmd_data_383, 1, 0},
  {0xD0, lcd_cmd_data_384, 1, 0},
  {0xD1, lcd_cmd_data_385, 1, 0},
  {0xD2, lcd_cmd_data_386, 1, 0},
  {0xD3, lcd_cmd_data_387, 1, 0},
  {0xD4, lcd_cmd_data_388, 1, 0},
  {0xD5, lcd_cmd_data_389, 1, 0},
  {0xD6, lcd_cmd_data_390, 1, 0},
  {0xD7, lcd_cmd_data_391, 1, 0},
  {0xD8, lcd_cmd_data_392, 1, 0},
  {0xD9, lcd_cmd_data_393, 1, 0},
  {0xF3, lcd_cmd_data_394, 1, 0},
  {0xF0, lcd_cmd_data_395, 1, 0},
  {0x35, lcd_cmd_data_396, 1, 0},
  {0x21, lcd_cmd_data_397, 1, 0},
  {0x11, lcd_cmd_data_398, 1, 120},
  {0x29, lcd_cmd_data_399, 1, 0},  
};

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    if (!ret_panel || !ret_io) return ESP_ERR_INVALID_ARG;
    if (panel || lcd_io) return ESP_ERR_INVALID_STATE;
    *ret_panel = nullptr;
    *ret_io = nullptr;
    esp_lcd_panel_handle_t new_panel = nullptr;
    esp_lcd_panel_io_handle_t new_io = nullptr;
    bool bus_created = false;
    auto create = [&]() -> esp_err_t {
        ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Backlight");
        // Preserve the board's reset pulse and version-dependent ST77916 tables.
        gpio_config_t reset = {};
        reset.pin_bit_mask = 1ULL << BSP_LCD_RST;
        reset.mode = GPIO_MODE_OUTPUT;
        ESP_RETURN_ON_ERROR(gpio_config(&reset), TAG, "LCD reset GPIO");
        ESP_RETURN_ON_ERROR(gpio_set_level(BSP_LCD_RST, 0), TAG, "LCD reset low");
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_RETURN_ON_ERROR(gpio_set_level(BSP_LCD_RST, 1), TAG, "LCD reset high");
        vTaskDelay(pdMS_TO_TICKS(10));
        spi_bus_config_t bus = {};
        bus.sclk_io_num = BSP_LCD_SCLK;
        bus.data0_io_num = BSP_LCD_DATA0;
        bus.data1_io_num = BSP_LCD_DATA1;
        bus.data2_io_num = BSP_LCD_DATA2;
        bus.data3_io_num = BSP_LCD_DATA3;
        bus.data4_io_num = bus.data5_io_num = bus.data6_io_num = bus.data7_io_num = -1;
        // The original Video player can submit one complete 360x360 frame.
        bus.max_transfer_sz = config && config->max_transfer_sz > 0 ? config->max_transfer_sz : BSP_LCD_H_RES * BSP_LCD_V_RES * 2;
        bus.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "LCD QSPI");
        bus_created = true;
        esp_lcd_panel_io_spi_config_t io = {};
        io.cs_gpio_num = BSP_LCD_CS;
        io.dc_gpio_num = -1;
        io.spi_mode = 0;
        io.pclk_hz = 3 * 1000 * 1000;
        io.trans_queue_depth = 10;
        io.lcd_cmd_bits = 32;
        io.lcd_param_bits = 8;
        io.flags.quad_mode = true;
        ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_HOST, &io, &new_io), TAG, "LCD ID IO");
        uint8_t id[4] = {};
        const int command = (0x0B << 24) | (0x04 << 8);
        const esp_err_t id_result = esp_lcd_panel_io_rx_param(new_io, command, id, sizeof(id));
        if (id_result != ESP_OK) ESP_LOGW(TAG, "LCD ID read: %s; using driver default table", esp_err_to_name(id_result));
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_del(new_io), TAG, "LCD ID IO cleanup");
        new_io = nullptr;
        io.pclk_hz = CONFIG_BSP_LCD_SPI_CLOCK_HZ;
        ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_HOST, &io, &new_io), TAG, "LCD IO");
        st77916_vendor_config_t vendor = {};
        vendor.flags.use_qspi_interface = true;
        if (id_result == ESP_OK && id[0] == 0 && id[1] == 0x7f && id[2] == 0x7f && id[3] == 0x7f) {
            vendor.init_cmds = vendor_specific_init_version_1;
            vendor.init_cmds_size = sizeof(vendor_specific_init_version_1) / sizeof(st77916_lcd_init_cmd_t);
        } else if (id_result == ESP_OK && id[0] == 0 && id[1] == 2 && id[2] == 0x7f && id[3] == 0x7f) {
            vendor.init_cmds = vendor_specific_init_version_2;
            vendor.init_cmds_size = sizeof(vendor_specific_init_version_2) / sizeof(st77916_lcd_init_cmd_t);
        }
        esp_lcd_panel_dev_config_t cfg = {};
        cfg.reset_gpio_num = BSP_LCD_RST;
        cfg.rgb_ele_order = BSP_LCD_COLOR_SPACE;
        cfg.bits_per_pixel = BSP_LCD_BITS_PER_PIXEL;
        cfg.vendor_config = &vendor;
        ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st77916(new_io, &cfg, &new_panel), TAG, "ST77916 create");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(new_panel), TAG, "LCD reset");
        return esp_lcd_panel_init(new_panel);
    };
    esp_err_t ret = create();
    if (ret != ESP_OK) {
        if (new_panel) esp_lcd_panel_del(new_panel);
        if (new_io) esp_lcd_panel_io_del(new_io);
        if (bus_created) spi_bus_free(BSP_LCD_SPI_HOST);
        return ret;
    }
    *ret_panel = new_panel;
    *ret_io = new_io;
    return ESP_OK;
}

esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    (void)config;
    if (!ret_touch) return ESP_ERR_INVALID_ARG;
    if (touch || touch_io) return ESP_ERR_INVALID_STATE;
    *ret_touch = nullptr;
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C");
    esp_lcd_panel_io_i2c_config_t io = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    io.scl_speed_hz = BSP_I2C_FREQ_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_bus, &io, &touch_io), TAG, "Touch IO");
    esp_lcd_touch_config_t cfg = {};
    cfg.x_max = BSP_LCD_H_RES;
    cfg.y_max = BSP_LCD_V_RES;
    cfg.rst_gpio_num = BSP_TOUCH_RST;
    cfg.int_gpio_num = BSP_TOUCH_INT;
    esp_err_t ret = esp_lcd_touch_new_i2c_cst816s(touch_io, &cfg, &touch);
    if (ret != ESP_OK) {
        touch = nullptr;
        esp_lcd_panel_io_del(touch_io);
        touch_io = nullptr;
        return ret;
    }
    *ret_touch = touch;
    return ESP_OK;
}

static lv_display_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    bsp_display_config_t config = {};
    if (bsp_display_new(&config, &panel, &lcd_io) != ESP_OK) return nullptr;
    esp_lv_adapter_display_config_t disp = {};
    disp.panel = panel;
    disp.panel_io = lcd_io;
    disp.profile.interface = ESP_LV_ADAPTER_PANEL_IF_OTHER;
    disp.profile.rotation = ESP_LV_ADAPTER_ROTATE_0;
    disp.profile.hor_res = BSP_LCD_H_RES;
    disp.profile.ver_res = BSP_LCD_V_RES;
    disp.profile.buffer_height = cfg->buffer_size / BSP_LCD_H_RES;
    disp.profile.use_psram = cfg->flags.buff_spiram;
    disp.profile.require_double_buffer = cfg->double_buffer;
    disp.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
    ESP_LOGI(TAG, "Display buffers: %u lines, internal RAM; no PSRAM-to-DMA bounce per full strip",
             unsigned(disp.profile.buffer_height));
    return esp_lv_adapter_register_display(&disp);
}

static lv_indev_t *bsp_display_indev_init(lv_display_t *disp)
{
    bsp_board_status.touch = bsp_touch_new(nullptr, &touch);
    if (bsp_board_status.touch != ESP_OK) return nullptr;
    const esp_lv_adapter_touch_config_t cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch);
    return esp_lv_adapter_register_touch(&cfg);
}

lv_display_t *bsp_display_start(void)
{
    bsp_display_cfg_t cfg = {};
    cfg.lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    cfg.buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_BUFFER_HEIGHT;
    cfg.double_buffer = true;
    cfg.flags.buff_spiram = false;
    return bsp_display_start_with_config(&cfg);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    if (display_started) return display;
    if (!cfg || !cfg->buffer_size || cfg->buffer_size % BSP_LCD_H_RES ||
        cfg->buffer_size > BSP_LCD_H_RES * BSP_LCD_V_RES) return nullptr;
    bsp_board_status.display = bsp_display_brightness_init();
    if (bsp_board_status.display != ESP_OK) return nullptr;
    bsp_board_status.i2c = bsp_i2c_init();
    if (bsp_board_status.i2c != ESP_OK) return nullptr;
    bsp_board_status.display = esp_lv_adapter_init(&cfg->lv_adapter_cfg);
    if (bsp_board_status.display != ESP_OK) return nullptr;
    port_initialized = true;
    display = bsp_display_lcd_init(cfg);
    if (!display) { bsp_board_status.display = ESP_FAIL; goto failed; }
    {
        if (!bsp_display_lock(0)) { bsp_board_status.display = ESP_FAIL; goto failed; }
        input = bsp_display_indev_init(display);
        if (!input) {
            if (bsp_board_status.touch == ESP_OK) bsp_board_status.touch = ESP_ERR_NO_MEM;
            ESP_LOGW(TAG, "Touch: %s", esp_err_to_name(bsp_board_status.touch));
            if (touch) { esp_lcd_touch_del(touch); touch = nullptr; }
            if (touch_io) { esp_lcd_panel_io_del(touch_io); touch_io = nullptr; }
        }
        bsp_display_unlock();
    }
    bsp_board_status.display = esp_lcd_panel_disp_on_off(panel, true);
    if (bsp_board_status.display != ESP_OK) goto failed;
    bsp_board_status.display = esp_lv_adapter_start();
    if (bsp_board_status.display != ESP_OK) goto failed;
    display_started = true;
    ESP_LOGI(TAG, "ST77916 %dx%d RGB565; LVGL %d.%d.%d, esp_lvgl_adapter, two draw buffers",
             BSP_LCD_H_RES, BSP_LCD_V_RES, LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    return display;
failed:
    ESP_LOGE(TAG, "Display startup failed: %s", esp_err_to_name(bsp_board_status.display));
    bsp_display_backlight_off();
    backlight_released = false;
    if (input) { esp_lv_adapter_unregister_touch(input); input = nullptr; }
    if (display) { esp_lv_adapter_unregister_display(display); display = nullptr; }
    if (port_initialized) { esp_lv_adapter_deinit(); port_initialized = false; }
    if (touch) { esp_lcd_touch_del(touch); touch = nullptr; }
    if (touch_io) { esp_lcd_panel_io_del(touch_io); touch_io = nullptr; }
    if (panel) { esp_lcd_panel_del(panel); panel = nullptr; }
    if (lcd_io) { esp_lcd_panel_io_del(lcd_io); lcd_io = nullptr; }
    spi_bus_free(BSP_LCD_SPI_HOST);
    return nullptr;
}

lv_display_t *bsp_display_get(void) { return display; }
lv_indev_t *bsp_display_get_input_dev(void) { return input; }
esp_lcd_touch_handle_t bsp_touch_get_handle(void) { return touch; }
void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation)
{
    lv_display_set_rotation(disp, rotation);
}
bool bsp_display_lock(uint32_t timeout_ms)
{
    return esp_lv_adapter_lock(timeout_ms == 0 || timeout_ms == UINT32_MAX ? -1 : (int32_t)timeout_ms) == ESP_OK;
}
void bsp_display_unlock(void) { esp_lv_adapter_unlock(); }
