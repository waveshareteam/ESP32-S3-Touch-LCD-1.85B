#ifndef RTC_BSP_H
#define RTC_BSP_H

#include <Arduino.h>
#include "SensorPCF85063.hpp"

typedef struct 
{
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t week;
}RtcDateTime_t;


void rtc_init(void);
void i2c_rtc_setTime(uint16_t year,uint8_t month,uint8_t day,uint8_t hour,uint8_t minute,uint8_t second);
RtcDateTime_t i2c_rtc_get(void);
void i2c_rtc_loop_task(void *arg);

#endif