/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "VideoPlayer.hpp"
#include "lvgl.h"
#include "esp_brookesia.hpp"

#include "esp_lib_utils.h"
#include "esp_check.h"
#include "esp_log.h"

#include "video_init/video_init.h"

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:VideoPlayer"

#define APP_NAME "VideoPlayer"

LV_IMG_DECLARE(img_app_vedioplayer);

namespace esp_brookesia::apps
{

    VideoPlayer *VideoPlayer::_instance = nullptr;

    VideoPlayer *VideoPlayer::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new VideoPlayer(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    VideoPlayer::VideoPlayer(bool use_status_bar, bool use_navigation_bar) : App(APP_NAME, &img_app_vedioplayer, true, use_status_bar, use_navigation_bar)
    {
    }
    VideoPlayer::~VideoPlayer()
    {
    }

    bool VideoPlayer::run(void)
    {
        ESP_UTILS_LOGD("Run");

        generic_file_list_t avi_files = {};
        esp_err_t err = bsp_sdcard_acquire();
        if (err == ESP_OK) {
            err = get_file_list_by_ext("/sdcard/video", ".avi", &avi_files);
            bsp_sdcard_release();
        }
        if (err == ESP_OK) {
            for (int i = 0; i < avi_files.count; i++) {
                printf("AVI[%d]: %s\n", i, avi_files.list[i]);
            }
        }
        init_video_ui_screen(lv_screen_active(),&avi_files);
        video_player_start(bsp_display_get(),NULL,1);
        for (int i = 0; i < avi_files.count; ++i) free(avi_files.list[i]);
        free(avi_files.list);
        return true;
    }
    
    bool VideoPlayer::back(void)
    {
        ESP_UTILS_LOGD("Back");
        // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
        ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
        return true;
    }

    bool VideoPlayer::close()
    {
        ESP_UTILS_LOGD("Close");
        return video_player_shutdown();
    }

    bool VideoPlayer::init()
    {
        ESP_UTILS_LOGD("Init");

        return true;
    }

    bool VideoPlayer::deinit()
    {
        ESP_UTILS_LOGD("Deinit");
        return true;
    }

    bool VideoPlayer::pause()
    {
        ESP_UTILS_LOGD("Pause");
        video_player_stop();
        return true;
    }

    bool VideoPlayer::resume()
    {
        ESP_UTILS_LOGD("Resume");

        return true;
    }


} // namespace esp_brookesia::apps
