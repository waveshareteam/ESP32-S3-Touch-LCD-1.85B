#include "bsp_board_extra.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include <atomic>
#include <algorithm>
#include "esp_timer.h"
#include "driver/gpio.h"

namespace {
SemaphoreHandle_t control_mutex;
std::atomic<bsp_extra_audio_owner_t> owner{BSP_EXTRA_AUDIO_OWNER_NONE};
std::atomic<uint8_t> volume{80};
esp_codec_dev_handle_t speaker, microphone;
esp_codec_dev_handle_t active_microphone;
bool opened, capturing;
i2s_slot_mode_t app_channels = I2S_SLOT_MODE_MONO;
std::atomic<TaskHandle_t> app_task{nullptr};
std::atomic<bsp_extra_audio_owner_t> app_owner{BSP_EXTRA_AUDIO_OWNER_NONE};

bool voice_owner()
{
    const auto current=owner.load();
    return current==BSP_EXTRA_AUDIO_OWNER_XIAOZHI || current==BSP_EXTRA_AUDIO_OWNER_RECORDER;
}

esp_err_t codec_result(int result)
{
    return result == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t close_codecs()
{
    if (!opened) return ESP_OK;
    esp_codec_dev_set_out_mute(speaker, true);
    esp_err_t mic_result = active_microphone ? codec_result(esp_codec_dev_close(active_microphone)) : ESP_OK;
    esp_err_t spk_result = codec_result(esp_codec_dev_close(speaker));
    if (mic_result != ESP_OK || spk_result != ESP_OK) return ESP_FAIL;
    opened = capturing = false;
    active_microphone = nullptr;
    return ESP_OK;
}

esp_err_t open_codecs(uint32_t rate, bool capture)
{
    const bool aec = capture && owner.load() == BSP_EXTRA_AUDIO_OWNER_XIAOZHI;
    active_microphone = capture ? (aec ? bsp_audio_codec_voice_microphone_init() : microphone) : nullptr;
    if (capture && !active_microphone) return ESP_ERR_NO_MEM;
    esp_codec_dev_sample_info_t fs = {};
    fs.sample_rate = rate;
    fs.channel = BSP_AUDIO_VOICE_CHANNELS;
    fs.bits_per_sample = aec ? 32 : 16;
    esp_err_t result = codec_result(esp_codec_dev_open(speaker, &fs));
    if (result == ESP_OK && capture) {
        fs.channel = aec ? BSP_AUDIO_VOICE_RX_CHANNELS : BSP_AUDIO_VOICE_CHANNELS;
        fs.bits_per_sample = 16;
        // Four ADC samples are packed into the two physical I2S slots.
        fs.channel_mask = aec ? ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0) | ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1) : 0;
        result = codec_result(esp_codec_dev_open(active_microphone, &fs));
    }
    if (result == ESP_OK) result = codec_result(esp_codec_dev_set_out_vol(speaker, volume.load()));
    if (result == ESP_OK && capture) result = codec_result(esp_codec_dev_set_in_gain(active_microphone,
        owner.load()==BSP_EXTRA_AUDIO_OWNER_RECORDER ? 24.0f : 30.0f));
    if (result == ESP_OK && aec) {
        result = codec_result(esp_codec_dev_set_in_channel_gain(
            active_microphone, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(2), 0.0f));
    }
    const bool raw_capture=capture && owner.load()==BSP_EXTRA_AUDIO_OWNER_RECORDER;
    if (result == ESP_OK) result = codec_result(esp_codec_dev_set_out_mute(speaker, raw_capture));
    // The DAC supplies the shared clocks, but raw microphone monitoring must
    // not drive the speaker. Its existing GPIO is configured by the codec.
    if (result == ESP_OK && owner.load()==BSP_EXTRA_AUDIO_OWNER_RECORDER)
        result=gpio_set_level(BSP_AUDIO_PA,raw_capture?0:1);
    if (result != ESP_OK) {
        if (active_microphone) esp_codec_dev_close(active_microphone);
        esp_codec_dev_close(speaker);
        active_microphone = nullptr;
        return result;
    }
    opened = true;
    capturing = capture;
    return ESP_OK;
}
}

extern "C" esp_err_t bsp_extra_codec_init(void)
{
    if (control_mutex) return ESP_OK;
    speaker = bsp_audio_codec_speaker_init();
    microphone = bsp_audio_codec_microphone_init();
    if (!speaker || !microphone) return ESP_FAIL;
    control_mutex = xSemaphoreCreateMutex();
    return control_mutex ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t codec_volume_set(int value)
{
    if (value < 0 || value > 100) return ESP_ERR_INVALID_ARG;
    if (!control_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(control_mutex, portMAX_DELAY);
    esp_err_t result = opened ? codec_result(esp_codec_dev_set_out_vol(speaker, value)) : ESP_OK;
    if (result == ESP_OK) volume.store(static_cast<uint8_t>(value));
    xSemaphoreGive(control_mutex);
    return result;
}

bsp_extra_audio_owner_t bsp_extra_audio_session_get_owner(void) { return owner.load(); }

const char *bsp_extra_audio_owner_name(bsp_extra_audio_owner_t value)
{
    switch (value) {
    case BSP_EXTRA_AUDIO_OWNER_MUSIC: return "Music";
    case BSP_EXTRA_AUDIO_OWNER_RECORDER: return "Recorder";
    case BSP_EXTRA_AUDIO_OWNER_XIAOZHI: return "Xiaozhi";
    default: return "None";
    }
}

esp_err_t bsp_extra_audio_session_acquire(bsp_extra_audio_owner_t value)
{
    if (!control_mutex) return ESP_ERR_INVALID_STATE;
    if (value <= BSP_EXTRA_AUDIO_OWNER_NONE || value >= BSP_EXTRA_AUDIO_OWNER_MAX) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(control_mutex, portMAX_DELAY);
    esp_err_t result = ESP_ERR_INVALID_STATE;
    if (owner.load() == BSP_EXTRA_AUDIO_OWNER_NONE) {
        owner.store(value);
        result = ESP_OK;
    }
    xSemaphoreGive(control_mutex);
    return result;
}

esp_err_t bsp_extra_audio_session_release(bsp_extra_audio_owner_t value)
{
    if (!control_mutex) return ESP_ERR_INVALID_STATE;
    if (value <= BSP_EXTRA_AUDIO_OWNER_NONE || value >= BSP_EXTRA_AUDIO_OWNER_MAX) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(control_mutex, portMAX_DELAY);
    esp_err_t result = ESP_ERR_INVALID_STATE;
    if (owner.load() == value && !opened) {
        owner.store(BSP_EXTRA_AUDIO_OWNER_NONE);
        result = ESP_OK;
    }
    xSemaphoreGive(control_mutex);
    return result;
}

esp_err_t bsp_audio_voice_open(uint32_t rate, bool capture)
{
    if (!control_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(control_mutex, portMAX_DELAY);
    esp_err_t result = ESP_ERR_INVALID_STATE;
    if (voice_owner() && !opened) result = open_codecs(rate, capture);
    xSemaphoreGive(control_mutex);
    if (result == ESP_OK) {
        if(owner.load()==BSP_EXTRA_AUDIO_OWNER_RECORDER)
            ESP_LOGI("bsp_audio","Mic Test: %lu Hz, %s",(unsigned long)rate,
                     capture?"raw MIC1/MIC2, stereo PCM16, PA off":"single microphone replay, stereo PCM16");
        else
            ESP_LOGI("bsp_audio","Xiaozhi ES8311 + ES7210: %lu Hz, %s",(unsigned long)rate,
                     capture?"AEC MIC1 + MIC2 + MIC3 reference, RX 4x16 / TX 2x32":"prompt TX 2x16");
    }
    return result;
}

esp_err_t bsp_audio_voice_read(void *pcm, size_t bytes, size_t *read, uint32_t timeout_ms)
{
    if (read) *read = 0;
    const size_t channels=owner.load()==BSP_EXTRA_AUDIO_OWNER_RECORDER?
                          BSP_AUDIO_VOICE_CHANNELS:BSP_AUDIO_VOICE_RX_CHANNELS;
    if (!pcm || !read || !bytes || bytes%(channels*sizeof(int16_t))) return ESP_ERR_INVALID_ARG;
    if (!voice_owner() || !opened || !capturing) return ESP_ERR_INVALID_STATE;
    return bsp_audio_i2s_read(pcm, bytes, read, timeout_ms);
}

esp_err_t bsp_audio_voice_write(void *pcm, size_t bytes, size_t *written, uint32_t timeout_ms)
{
    if (written) *written = 0;
    const bool aec=capturing && owner.load()==BSP_EXTRA_AUDIO_OWNER_XIAOZHI;
    const size_t frame_bytes=BSP_AUDIO_VOICE_CHANNELS*(aec?sizeof(int32_t):sizeof(int16_t));
    if (!pcm || !written || !bytes || bytes%frame_bytes) return ESP_ERR_INVALID_ARG;
    if (!voice_owner() || !opened) return ESP_ERR_INVALID_STATE;
    return bsp_audio_i2s_write(pcm, bytes, written, timeout_ms);
}

esp_err_t bsp_audio_voice_mute(bool mute)
{
    if (!control_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(control_mutex, portMAX_DELAY);
    esp_err_t result=voice_owner() && opened?
                     codec_result(esp_codec_dev_set_out_mute(speaker,mute)):ESP_ERR_INVALID_STATE;
    if(result==ESP_OK && owner.load()==BSP_EXTRA_AUDIO_OWNER_RECORDER)
        result=gpio_set_level(BSP_AUDIO_PA,(!mute && !capturing)?1:0);
    xSemaphoreGive(control_mutex);
    return result;
}

esp_err_t bsp_audio_voice_close(void)
{
    if (!control_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(control_mutex, portMAX_DELAY);
    esp_err_t result = voice_owner() ? close_codecs() : ESP_ERR_INVALID_STATE;
    xSemaphoreGive(control_mutex);
    return result;
}

bool bsp_extra_audio_acquire(bsp_extra_audio_owner_t value, uint32_t timeout_ms)
{
    if (!control_mutex || (value != BSP_EXTRA_AUDIO_OWNER_MUSIC && value != BSP_EXTRA_AUDIO_OWNER_RECORDER)) return false;
    const int64_t deadline = esp_timer_get_time() + int64_t(timeout_ms) * 1000;
    do {
        xSemaphoreTake(control_mutex, portMAX_DELAY);
        bool available = owner.load() == BSP_EXTRA_AUDIO_OWNER_NONE;
        if (available) {
            owner.store(value);
            app_owner.store(value);
            app_task.store(xTaskGetCurrentTaskHandle());
        }
        xSemaphoreGive(control_mutex);
        if (available) return true;
        if (timeout_ms && esp_timer_get_time() >= deadline) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    } while (true);
}

void bsp_extra_audio_release(void)
{
    if (!control_mutex || app_task.load() != xTaskGetCurrentTaskHandle()) return;
    xSemaphoreTake(control_mutex, portMAX_DELAY);
    if (app_owner.load() != BSP_EXTRA_AUDIO_OWNER_NONE && owner.load() == app_owner.load() && close_codecs() == ESP_OK) {
        app_task.store(nullptr);
        app_owner.store(BSP_EXTRA_AUDIO_OWNER_NONE);
        owner.store(BSP_EXTRA_AUDIO_OWNER_NONE);
    }
    xSemaphoreGive(control_mutex);
}

esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits, i2s_slot_mode_t channels)
{
    if (bits != 16 || (channels != I2S_SLOT_MODE_MONO && channels != I2S_SLOT_MODE_STEREO) || rate < 8000 || rate > 48000) return ESP_ERR_INVALID_ARG;
    if (app_task.load() != xTaskGetCurrentTaskHandle()) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(control_mutex, portMAX_DELAY);
    esp_err_t result = close_codecs();
    if (result == ESP_OK) result = open_codecs(rate, app_owner.load() == BSP_EXTRA_AUDIO_OWNER_RECORDER);
    if (result == ESP_OK) app_channels = channels;
    xSemaphoreGive(control_mutex);
    return result;
}
esp_err_t bsp_extra_codec_dev_stop(void)
{
    if (app_task.load() != xTaskGetCurrentTaskHandle()) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(control_mutex, portMAX_DELAY);
    esp_err_t result = close_codecs();
    xSemaphoreGive(control_mutex);
    return result;
}
esp_err_t bsp_extra_codec_volume_set(int value, int *applied)
{
    const esp_err_t result = codec_volume_set(value);
    if (result == ESP_OK && applied) *applied = volume.load();
    return result;
}
int bsp_extra_codec_volume_get(void) { return volume.load(); }
esp_err_t bsp_extra_audio_drain(void)
{
    if (app_task.load() != xTaskGetCurrentTaskHandle()) return ESP_ERR_INVALID_STATE;
    // More than one complete six-descriptor DMA ring at the lowest supported rate.
    vTaskDelay(pdMS_TO_TICKS(200));
    return ESP_OK;
}
esp_err_t bsp_extra_i2s_write(void *pcm, size_t bytes, size_t *written, uint32_t timeout_ms)
{
    if (written) *written=0;
    const size_t channels=app_channels==I2S_SLOT_MODE_STEREO?2:1;
    if(!pcm || !bytes || bytes%(channels*sizeof(int16_t))) return ESP_ERR_INVALID_ARG;
    if(app_task.load()!=xTaskGetCurrentTaskHandle() || !opened) return ESP_ERR_INVALID_STATE;
    int16_t stereo[512];
    const auto *input=static_cast<const int16_t *>(pcm);
    const int64_t deadline=esp_timer_get_time()+int64_t(timeout_ms)*1000;
    size_t offset=0;
    while(offset<bytes/(2*channels)) {
        const size_t frames=std::min<size_t>(256,bytes/(2*channels)-offset);
        for(size_t i=0;i<frames;++i) {
            const size_t index=(offset+i)*channels;
            // The board has one speaker: replay both microphone channels.
            const int16_t sample=channels==2?(int32_t(input[index])+input[index+1])/2:input[index];
            stereo[i*2]=stereo[i*2+1]=sample;
        }
        size_t sent=0;
        const int64_t remaining=deadline-esp_timer_get_time();
        if(remaining<=0) return ESP_ERR_TIMEOUT;
        const esp_err_t ret=bsp_audio_i2s_write(stereo,frames*4,&sent,(remaining+999)/1000);
        if(ret!=ESP_OK || sent!=frames*4) return ret==ESP_OK?ESP_FAIL:ret;
        offset+=frames;
        if(written) *written=offset*2*channels;
    }
    return ESP_OK;
}
esp_err_t bsp_extra_i2s_read(void *pcm, size_t bytes, size_t *read, uint32_t timeout_ms)
{
    if (read) *read = 0;
    if (!pcm || !bytes || bytes % sizeof(int16_t)) return ESP_ERR_INVALID_ARG;
    if (app_task.load() != xTaskGetCurrentTaskHandle() || !opened || !capturing) return ESP_ERR_INVALID_STATE;
    if (app_channels == I2S_SLOT_MODE_STEREO) {
        if (bytes % 4) return ESP_ERR_INVALID_ARG;
        size_t count = 0;
        const esp_err_t ret = bsp_audio_i2s_read(pcm, bytes, &count, timeout_ms);
        if (read) *read = count;
        return ret == ESP_OK && count != bytes ? ESP_FAIL : ret;
    }
    int16_t stereo[512];
    auto *mono = static_cast<int16_t *>(pcm);
    const int64_t deadline = esp_timer_get_time() + int64_t(timeout_ms) * 1000;
    size_t offset = 0;
    while (offset < bytes / 2) {
        const size_t frames = std::min<size_t>(256, bytes / 2 - offset);
        size_t received = 0;
        const int64_t remaining = deadline - esp_timer_get_time();
        if (remaining <= 0) return ESP_ERR_TIMEOUT;
        const esp_err_t ret = bsp_audio_i2s_read(stereo, frames * 4, &received, (remaining + 999) / 1000);
        if (ret != ESP_OK || received != frames * 4) return ret == ESP_OK ? ESP_FAIL : ret;
        for (size_t i = 0; i < frames; ++i) mono[offset + i] = stereo[i * 2];
        offset += frames;
        if (read) *read = offset * 2;
    }
    return ESP_OK;
}
