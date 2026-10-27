/* SPDX-License-Identifier: Apache-2.0 */
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    while (true) {
        bsp_battery_data_t data{};
        esp_err_t ret = bsp_battery_init();
        if (ret == ESP_OK) ret = bsp_battery_get_data(&data);
        if (ret != ESP_OK) ESP_LOGW("battery", "Read: %s", esp_err_to_name(ret));
        else {
            ESP_LOGI("battery", "Status=0x%04x operation=0x%04x present=%d initialized=%d",
                data.battery_status, data.operation_status, data.present, data.initialized);
            if (data.present && data.initialized)
                ESP_LOGI("battery", "%u%% %u mV %+d mA; RC=%u FCC=%u design=%u mAh; avg=%+d mA; gauge=%.1f C; cycles=%u",
                    data.percent, data.voltage_mv, data.current_ma, data.remaining_mah, data.full_mah,
                    data.design_mah, data.average_current_ma, data.temperature_c, data.cycles);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
