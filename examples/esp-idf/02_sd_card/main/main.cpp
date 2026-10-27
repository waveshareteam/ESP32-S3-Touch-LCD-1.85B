/* SPDX-License-Identifier: Apache-2.0 */
#include <cstdio>
#include <cstring>
#include "bsp/esp-bsp.h"
#include "esp_log.h"

extern "C" void app_main(void)
{
    // Initialize the BSP locks used by the SD card lease API.
    ESP_ERROR_CHECK(bsp_i2c_init());
    esp_err_t ret = bsp_sdcard_acquire();
    if (ret != ESP_OK) {
        ESP_LOGE("sd_card", "Mount: %s. Insert a FAT32 card and reset.", esp_err_to_name(ret));
        return;
    }
    bsp_sdcard_info_t info{};
    if (bsp_sdcard_get_info(&info) == ESP_OK)
        ESP_LOGI("sd_card", "Capacity=%llu MiB, filesystem=%llu MiB, free=%llu MiB",
            info.capacity_bytes/1048576, info.total_bytes/1048576, info.free_bytes/1048576);
    constexpr char path[] = BSP_SD_MOUNT_POINT "/bsp_sd_test.txt";
    constexpr char content[] = "ESP32-S3-Touch-LCD-1.85B SD test\n";
    bool passed = false;
    FILE *file = std::fopen(path, "wb");
    if (file) {
        const size_t written = std::fwrite(content, 1, sizeof(content)-1, file);
        const int closed = std::fclose(file);
        if (written == sizeof(content)-1 && closed == 0) {
            file = std::fopen(path, "rb");
            if (file) {
                char readback[sizeof(content)]{};
                const size_t read = std::fread(readback, 1, sizeof(readback), file);
                const bool failed = std::ferror(file);
                const int read_closed = std::fclose(file);
                passed = !failed && read_closed == 0 && read == sizeof(content)-1 &&
                         std::memcmp(readback, content, read) == 0;
            }
        }
    }
    ESP_LOGI("sd_card", "%s: write/read %s", path, passed ? "PASS" : "FAIL");
    bsp_sdcard_release();
    ret = bsp_sdcard_unmount();
    if (ret != ESP_OK) ESP_LOGE("sd_card", "Unmount: %s", esp_err_to_name(ret));
}
