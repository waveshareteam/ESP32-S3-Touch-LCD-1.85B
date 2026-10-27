#include "qmi8658c.h"
#include "I2c_bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
void setup()
{
  Serial.begin(115200);
  I2C_master_Init();
  xTaskCreate(qmi8658c_example,"PCF85063_example", 3000, NULL , 2, NULL);
}
void loop()
{
  
}
