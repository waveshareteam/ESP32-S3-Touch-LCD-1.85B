#include "rtc_bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void setup()
{
  Serial.begin(115200);
  rtc_init(); //rtc init
  i2c_rtc_setTime(2025,9,9,14,51,30);//Set the RTC time to June 26, 2025, 14:51:30.
  xTaskCreatePinnedToCore(i2c_rtc_loop_task, "i2c_rtc_loop_task", 3 * 1024, NULL , 2, NULL,0); //rtc read data
}

void loop() 
{
  
}
