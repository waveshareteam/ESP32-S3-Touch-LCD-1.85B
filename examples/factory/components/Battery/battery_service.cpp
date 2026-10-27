#include "battery_service.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace {
portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
BatterySnapshot snapshot;
TaskHandle_t worker_handle;

void worker(void *)
{
    esp_err_t last_error = ESP_ERR_NOT_FINISHED;
    BatteryState last_state = BatteryState::Reading;
    for (;;) {
        BatterySnapshot next;
        next.error = bsp_battery_init();
        if (next.error == ESP_OK) next.error = bsp_battery_get_data(&next.data);
        next.sampled_us = esp_timer_get_time();
        const auto state = battery_state(next, next.sampled_us);
        if (next.error != last_error || state != last_state) {
            if (next.error == ESP_OK) ESP_LOGI("battery", "%s", battery_state_text(state));
            else ESP_LOGW("battery", "BQ27220 read failed: %s", esp_err_to_name(next.error));
            last_error = next.error;
            last_state = state;
        }
        portENTER_CRITICAL(&mutex);
        snapshot = next;
        portEXIT_CRITICAL(&mutex);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
}

bool battery_service_start()
{
    if (worker_handle) return true;
    if (xTaskCreatePinnedToCoreWithCaps(worker, "battery", 4096, nullptr, 1, &worker_handle,
        tskNO_AFFINITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) == pdPASS) return true;
    portENTER_CRITICAL(&mutex);
    snapshot.error = ESP_ERR_NO_MEM;
    portEXIT_CRITICAL(&mutex);
    return false;
}

BatterySnapshot battery_service_snapshot()
{
    portENTER_CRITICAL(&mutex);
    auto result = snapshot;
    portEXIT_CRITICAL(&mutex);
    return result;
}
