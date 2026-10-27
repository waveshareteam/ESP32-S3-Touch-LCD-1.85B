#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "pcf85063a.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "pcf85063a_example";

// Initial RTC time to be set
static pcf85063a_datetime_t Set_Time = {
    .year = 2025,
    .month = 07,
    .day = 30,
    .dotw = 3,   // Day of the week: 0 = Sunday
    .hour = 9,
    .min = 0,
    .sec = 0
};

// Alarm time to be set
static pcf85063a_datetime_t Set_Alarm_Time = {
    .year = 2025,
    .month = 07,
    .day = 30,
    .dotw = 3,
    .hour = 9,
    .min = 0,
    .sec = 2
};

char datetime_str[256];  // Buffer to store formatted date-time string



static void pcf85063a_test_task(void *arg) {
    i2c_master_bus_handle_t bus_handle = (i2c_master_bus_handle_t)arg;
    pcf85063a_dev_t dev;
    pcf85063a_datetime_t Now_time;

    ESP_LOGI(TAG, "Initializing PCF85063A...");
    esp_err_t ret = pcf85063a_init(&dev, bus_handle, PCF85063A_ADDRESS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize PCF85063A (error: %d)", ret);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "Set current time.");
    pcf85063a_set_time_date(&dev, Set_Time);

    while (1) {
        
        // Read current time from RTC
        pcf85063a_get_time_date(&dev, &Now_time);

        // Format current time as a string
        pcf85063a_datetime_to_str(datetime_str, Now_time);
        ESP_LOGI(TAG, "Now_time is %s", datetime_str);

        // Wait for 1 second
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing I2C...");
    bsp_i2c_init();
    i2c_master_bus_handle_t bus_handle = bsp_i2c_get_handle();
    xTaskCreate(pcf85063a_test_task, "pcf85063a_test_task", 4096, bus_handle, 5, NULL);
}