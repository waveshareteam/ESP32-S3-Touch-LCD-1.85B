#pragma once
#include <stdint.h>
#include "bsp/esp-bsp.h"

constexpr unsigned SETTINGS_SCAN_PAGE_SIZE = 4;
struct settings_ap_t {
    char ssid[33];
    char security[16];
    int rssi;
    unsigned channel;
};
struct settings_snapshot_t {
    int brightness, volume;
    esp_err_t controls_error, save_error;
    bool controls_pending;
    bool scan_busy, scan_checked;
    esp_err_t scan_error;
    uint32_t scan_revision;
    uint16_t scan_count, scan_page_count;
    settings_ap_t networks[SETTINGS_SCAN_PAGE_SIZE];
    bool sd_busy, sd_checked;
    esp_err_t sd_error;
    bsp_sdcard_info_t sd;
    char firmware[33], sdk[24], mac[18];
    uint32_t flash_mb, psram_mb;
};

// Call once after BSP startup and before opening APPs. Worker never accesses LVGL.
bool settings_service_start();
void settings_service_snapshot(settings_snapshot_t *out, uint16_t scan_first = 0);
void settings_service_set_brightness(int percent, bool commit);
void settings_service_set_volume(int percent, bool commit);
bool settings_service_scan();
bool settings_service_check_sd();
