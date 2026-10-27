/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "bsp/esp-bsp.h"
#include "esp_log.h"

static const char *TAG = "sd_card";
static const char test_text[] = "ESP32-S3-Touch-LCD-1.85B SD test\n";
static const char test_path[] = BSP_SD_MOUNT_POINT "/test.txt";

typedef struct {
    esp_err_t error;
    const char *status;
    const char *hint;
    char contents[sizeof(test_text)];
} sd_test_result_t;

static void run_sd_test(sd_test_result_t *result)
{
    result->status = "SD card not mounted";
    result->hint = "Insert a FAT32 microSD card,\nthen press RESET to retry.";
    // Initialize the locks used by the board SD card lease API.
    result->error = bsp_i2c_init();
    if (result->error == ESP_OK) result->error = bsp_sdcard_acquire();
    if (result->error != ESP_OK) {
        ESP_LOGE(TAG, "Mount failed: %s. Insert a FAT32 microSD card and press RESET.",
                 esp_err_to_name(result->error));
        return;
    }
    ESP_LOGI(TAG, "SD card mounted at %s", BSP_SD_MOUNT_POINT);

    bsp_sdcard_info_t info{};
    if (bsp_sdcard_get_info(&info) == ESP_OK) {
        ESP_LOGI(TAG, "Capacity=%llu MiB, filesystem=%llu MiB, free=%llu MiB",
                 info.capacity_bytes / 1048576, info.total_bytes / 1048576,
                 info.free_bytes / 1048576);
    }

    result->error = ESP_FAIL;
    result->status = "File write failed";
    result->hint = "Check the card is writable,\nthen press RESET to retry.";
    size_t bytes = 0;
    int closed = 0;
    int extra = EOF;
    bool read_error = false;
    FILE *file = fopen(test_path, "wb");
    if (!file) {
        ESP_LOGE(TAG, "Open for writing failed: %s", strerror(errno));
        goto cleanup;
    }
    bytes = fwrite(test_text, 1, sizeof(test_text) - 1, file);
    closed = fclose(file);
    if (bytes != sizeof(test_text) - 1 || closed != 0) {
        ESP_LOGE(TAG, "Write or close failed: %s", test_path);
        goto cleanup;
    }
    ESP_LOGI(TAG, "Write successful: %s (%u bytes)", test_path, (unsigned)bytes);

    result->status = "File read failed";
    result->hint = "Check the card, then press\nRESET to repeat the test.";
    file = fopen(test_path, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Open for reading failed: %s", strerror(errno));
        goto cleanup;
    }
    bytes = fread(result->contents, 1, sizeof(result->contents) - 1, file);
    extra = fgetc(file);
    read_error = ferror(file);
    closed = fclose(file);
    result->contents[bytes] = '\0';
    if (read_error || closed != 0 || extra != EOF || bytes != sizeof(test_text) - 1 ||
        memcmp(test_text, result->contents, bytes) != 0) {
        result->contents[0] = '\0';
        ESP_LOGE(TAG, "Read failed or file content did not match: %s", test_path);
        goto cleanup;
    }
    ESP_LOGI(TAG, "Read successful: %s", result->contents);
    result->error = ESP_OK;
    result->status = "Write / read passed";
    result->hint = "Test complete.\nPress RESET to run again.";

cleanup:
    ESP_LOGI(TAG, "Write/read verification: %s", esp_err_to_name(result->error));
    bsp_sdcard_release();
    esp_err_t unmount = bsp_sdcard_unmount();
    if (unmount == ESP_OK) {
        ESP_LOGI(TAG, "SD card unmounted; test.txt remains on the card");
    } else {
        ESP_LOGW(TAG, "Unmount failed: %s", esp_err_to_name(unmount));
        if (result->error == ESP_OK) {
            result->error = unmount;
            result->status = "Card cleanup failed";
            result->hint = "Read/write passed. Check\nthe serial log, then RESET.";
        }
    }
}

static void show_result(const sd_test_result_t *result)
{
    if (!bsp_display_start()) {
        ESP_LOGE(TAG, "Display initialization failed; check the serial test result above");
        return;
    }
    if (!bsp_display_lock(UINT32_MAX)) {
        ESP_LOGE(TAG, "Display lock unavailable");
        return;
    }

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xf3f6fa), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0x203040), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "SD Card Test");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 44);

    lv_obj_t *status = lv_label_create(screen);
    lv_obj_set_width(status, 240);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status,
        lv_color_hex(result->error == ESP_OK ? 0x16804a : 0xb63636), 0);
    lv_label_set_text_fmt(status, "%s\n%s", result->status, esp_err_to_name(result->error));
    lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t *card = lv_obj_create(screen);
    lv_obj_set_size(card, 240, 116);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 132);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *path = lv_label_create(card);
    lv_label_set_text(path, test_path);
    lv_obj_set_style_text_font(path, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(path, lv_color_hex(0x637487), 0);
    lv_obj_align(path, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *contents = lv_label_create(card);
    lv_obj_set_width(contents, 216);
    lv_obj_set_style_text_font(contents, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(contents, LV_LABEL_LONG_WRAP);
    lv_label_set_text(contents, result->contents[0] ? result->contents : "No data read from the card.");
    lv_obj_align(contents, LV_ALIGN_TOP_LEFT, 0, 32);

    lv_obj_t *hint = lv_label_create(screen);
    lv_obj_set_width(hint, 240);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x637487), 0);
    lv_label_set_text(hint, result->hint);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 264);
    bsp_display_unlock();

    esp_err_t ret = bsp_display_show();
    if (ret != ESP_OK) ESP_LOGE(TAG, "First frame display failed: %s", esp_err_to_name(ret));
}

extern "C" void app_main(void)
{
    sd_test_result_t result{};
    // Finish the one-shot card test, then keep its result visible on the round LCD.
    run_sd_test(&result);
    show_result(&result);
}
