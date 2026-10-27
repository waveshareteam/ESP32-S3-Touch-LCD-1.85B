/* SPDX-License-Identifier: Apache-2.0 */
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    // Set true to demonstrate setting the calendar on each boot.
    constexpr bool SET_TIME_ON_BOOT = false;
    const bsp_sensor_datetime_t initial = {2026, 1, 1, 12, 0, 0, 4};
    bool ready = false;
    while (true) {
        esp_err_t ret = ready ? ESP_OK : bsp_rtc_init();
        if (ret == ESP_OK && !ready) {
            if (SET_TIME_ON_BOOT) ret = bsp_rtc_set_datetime(&initial);
            ready = ret == ESP_OK;
        }
        bsp_sensor_datetime_t date{};
        if (ret == ESP_OK) ret = bsp_rtc_get_datetime(&date);
        if (ret == ESP_OK)
            ESP_LOGI("rtc", "%04u-%02u-%02u %02u:%02u:%02u", date.year, date.month, date.day,
                date.hour, date.minute, date.second);
        else { ESP_LOGW("rtc", "Read: %s", esp_err_to_name(ret)); ready = false; }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
