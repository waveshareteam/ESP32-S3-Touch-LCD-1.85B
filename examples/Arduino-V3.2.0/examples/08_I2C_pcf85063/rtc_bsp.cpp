#include <stdio.h>
#include <Wire.h>
#include "rtc_bsp.h"
#include "esp_log.h"

#ifndef SENSOR_SDA
#define SENSOR_SDA  11
#endif

#ifndef SENSOR_SCL
#define SENSOR_SCL  10
#endif

SensorPCF85063 rtc;


void rtc_init(void)
{
  if (!rtc.begin(Wire, SENSOR_SDA, SENSOR_SCL))
  {
    Serial.println("Failed to find PCF85063 - check your wiring!");
    while (1)
    {
      delay(1000);
    }
  }
}

/*
uint16_t year = 2023;
uint8_t month = 9;
uint8_t day = 7;
uint8_t hour = 11;
uint8_t minute = 24;
uint8_t second = 30;
*/
void i2c_rtc_setTime(uint16_t year,uint8_t month,uint8_t day,uint8_t hour,uint8_t minute,uint8_t second)
{
  rtc.setDateTime(year, month, day, hour, minute, second);
}


RtcDateTime_t i2c_rtc_get(void)
{
  RtcDateTime_t time;
  RTC_DateTime datetime = rtc.getDateTime();
  time.year = datetime.getYear();
  time.month = datetime.getMonth();
  time.day = datetime.getDay();
  time.hour = datetime.getHour();
  time.minute = datetime.getMinute();
  time.second = datetime.getSecond();
  time.week = datetime.getWeek();
  return time;
}

/*test task*/
void i2c_rtc_loop_task(void *arg)
{
  for(;;)
  {
    RTC_DateTime datetime = rtc.getDateTime();
    Serial.printf("%d/%d/%d %d:%d:%d\n",datetime.getYear(),datetime.getMonth(),datetime.getDay(),datetime.getHour(),datetime.getMinute(),datetime.getSecond(),datetime.getWeek());  
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


