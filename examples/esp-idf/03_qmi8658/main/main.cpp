/* SPDX-License-Identifier: Apache-2.0 */
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(bsp_imu_init());
    while (true) {
        bsp_imu_data_t data{};
        const esp_err_t ret = bsp_imu_get_data(&data);
        if (ret == ESP_OK)
            ESP_LOGI("qmi8658", "Accel [g]: %.3f %.3f %.3f; gyro [deg/s]: %.2f %.2f %.2f; die: %.1f C",
                data.accel_x, data.accel_y, data.accel_z, data.gyro_x, data.gyro_y, data.gyro_z, data.temperature_c);
        else ESP_LOGW("qmi8658", "Read: %s", esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
