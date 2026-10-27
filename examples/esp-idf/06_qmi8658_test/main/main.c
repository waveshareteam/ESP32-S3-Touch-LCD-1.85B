#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"

qmi8658_dev_t *qmi8658_dev = NULL;

void app_main(void)
{

    qmi8658_dev = bsp_qmi8658_drv_init();
    
    while (1)
    {
    qmi8658_data_t data;
    bool ready;
    esp_err_t ret = qmi8658_is_data_ready(qmi8658_dev, &ready);
    if (ret == ESP_OK && ready) 
    {
        ret = qmi8658_read_sensor_data(qmi8658_dev, &data);
        if (ret == ESP_OK) 
        {
            ESP_LOGI("TAG", "Accel: X=%.4f m/s², Y=%.4f m/s², Z=%.4f m/s²",data.accelX, data.accelY, data.accelZ);
            ESP_LOGI("TAG", "Gyro:  X=%.4f rad/s, Y=%.4f rad/s, Z=%.4f rad/s",data.gyroX, data.gyroY, data.gyroZ);
            ESP_LOGI("TAG", "Temp:  %.2f °C, Timestamp: %lu",data.temperature, data.timestamp);

        } 
    } 
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}
