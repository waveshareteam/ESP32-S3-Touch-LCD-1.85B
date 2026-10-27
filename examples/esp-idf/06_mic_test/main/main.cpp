/* SPDX-License-Identifier: Apache-2.0 */
#include "MicTest.hpp"
#include "bsp_board_extra.h"
#include "esp_log.h"

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(bsp_display_start() ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(bsp_extra_codec_init());
    ESP_ERROR_CHECK(bsp_extra_codec_volume_set(60, nullptr));
    static audio_demo::MicTest microphone;
    if (!bsp_display_lock(UINT32_MAX)) return;
    const bool ready = microphone.init() && microphone.run();
    bsp_display_unlock();
    if (!ready) { ESP_LOGE("audio", "Microphone screen initialization failed"); return; }
    ESP_ERROR_CHECK(bsp_display_show());
}
