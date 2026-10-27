#include "bsp_board_extra.h"
#include "settings_service.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "esp_app_desc.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "wifi_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

namespace {
portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t worker_handle;
settings_snapshot_t state = {};
bool sd_requested, brightness_requested, volume_requested, save_requested;
uint32_t controls_revision;

esp_err_t save_controls(int brightness, int volume)
{
    nvs_handle_t nvs;
    esp_err_t ret = nvs_open("ui_settings", NVS_READWRITE, &nvs);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_u8(nvs, "brightness", brightness);
    if (ret == ESP_OK) ret = nvs_set_u8(nvs, "volume", volume);
    if (ret == ESP_OK) ret = nvs_commit(nvs);
    nvs_close(nvs);
    return ret;
}

void check_sd()
{
    bsp_sdcard_info_t info = {};
    esp_err_t ret = bsp_sdcard_acquire();
    if (ret == ESP_OK) {
        ret = bsp_sdcard_get_info(&info);
        bsp_sdcard_release();
    }
    portENTER_CRITICAL(&mutex);
    state.sd = info;
    state.sd_error = ret;
    state.sd_busy = false;
    state.sd_checked = true;
    portEXIT_CRITICAL(&mutex);
}

void worker(void *)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        portENTER_CRITICAL(&mutex);
        bool do_sd = sd_requested;
        bool do_brightness = brightness_requested, do_volume = volume_requested, do_save = save_requested;
        int brightness = state.brightness, volume = state.volume;
        uint32_t revision = controls_revision;
        sd_requested = brightness_requested = volume_requested = save_requested = false;
        portEXIT_CRITICAL(&mutex);
        esp_err_t ret = ESP_OK;
        if (do_brightness) ret = bsp_display_brightness_set(brightness);
        if (do_volume) {
            esp_err_t audio_ret = bsp_extra_codec_volume_set(volume, nullptr);
            if (ret == ESP_OK) ret = audio_ret;
        }
        esp_err_t saved = do_save && ret == ESP_OK
            ? save_controls(bsp_display_brightness_get(), bsp_extra_codec_volume_get()) : ret;
        portENTER_CRITICAL(&mutex);
        if (revision == controls_revision) {
            if (do_brightness || do_volume) { state.controls_pending = false; state.controls_error = ret; }
            if (do_save) state.save_error = saved;
        }
        portEXIT_CRITICAL(&mutex);
        if (do_sd) check_sd();
    }
}
}

bool settings_service_start()
{
    if (worker_handle) return true;
    state.brightness = bsp_display_brightness_get();
    state.volume = bsp_extra_codec_volume_get();
    nvs_handle_t nvs;
    if (nvs_open("ui_settings", NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t value;
        if (nvs_get_u8(nvs, "brightness", &value) == ESP_OK && value >= 10 && value <= 100) {
            if (bsp_display_brightness_set(value) == ESP_OK) state.brightness = value;
        }
        if (nvs_get_u8(nvs, "volume", &value) == ESP_OK && value <= 100) {
            if (bsp_extra_codec_volume_set(value, nullptr) == ESP_OK) state.volume = value;
        }
        nvs_close(nvs);
    }
    uint8_t mac[6] = {};
    uint32_t flash = 0;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    esp_flash_get_size(nullptr, &flash);
    std::snprintf(state.mac, sizeof(state.mac), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    std::snprintf(state.firmware, sizeof(state.firmware), "%s", esp_app_get_description()->version);
    std::snprintf(state.sdk, sizeof(state.sdk), "%s", esp_get_idf_version());
    state.flash_mb = flash / (1024 * 1024);
    state.psram_mb = esp_psram_get_size() / (1024 * 1024);
    return xTaskCreate(worker, "settings", 4096, nullptr, 2, &worker_handle) == pdPASS;
}

void settings_service_snapshot(settings_snapshot_t *out, uint16_t first)
{
    if (!out) return;
    portENTER_CRITICAL(&mutex);
    *out = state;
    portEXIT_CRITICAL(&mutex);
    if (!out->controls_pending) {
        out->brightness = bsp_display_brightness_get();
        out->volume = bsp_extra_codec_volume_get();
    }
    wifi_scan_snapshot_t scan={};
    wifi_manager_get_scan_snapshot(&scan,first);
    out->scan_busy=scan.running; out->scan_error=static_cast<esp_err_t>(scan.error);
    out->scan_count=scan.count; out->scan_revision=scan.revision;
    out->scan_page_count=scan.page_count;
    for(unsigned i=0;i<scan.page_count;++i) {
        std::snprintf(out->networks[i].ssid,sizeof(out->networks[i].ssid),"%s",scan.records[i].ssid);
        std::snprintf(out->networks[i].security,sizeof(out->networks[i].security),"%s",scan.records[i].security);
        out->networks[i].rssi=scan.records[i].rssi; out->networks[i].channel=scan.records[i].channel;
    }
}

void settings_service_set_brightness(int percent, bool commit)
{
    if (!worker_handle) return;
    portENTER_CRITICAL(&mutex);
    state.brightness = std::clamp(percent, 10, 100);
    state.controls_pending = brightness_requested = true;
    save_requested |= commit;
    ++controls_revision;
    portEXIT_CRITICAL(&mutex);
    xTaskNotifyGive(worker_handle);
}

void settings_service_set_volume(int percent, bool commit)
{
    if (!worker_handle) return;
    portENTER_CRITICAL(&mutex);
    state.volume = std::clamp(percent, 0, 100);
    state.controls_pending = volume_requested = true;
    save_requested |= commit;
    ++controls_revision;
    portEXIT_CRITICAL(&mutex);
    xTaskNotifyGive(worker_handle);
}

bool settings_service_scan()
{
    return wifi_manager_request_scan();
}

bool settings_service_check_sd()
{
    if (!worker_handle) return false;
    portENTER_CRITICAL(&mutex);
    bool accepted = !state.sd_busy;
    if (accepted) { state.sd_busy = true; sd_requested = true; }
    portEXIT_CRITICAL(&mutex);
    if (accepted) xTaskNotifyGive(worker_handle);
    return accepted;
}
