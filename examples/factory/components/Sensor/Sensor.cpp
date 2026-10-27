#include "Sensor.hpp"
#include <cstdio>
#include <new>
#include "esp_timer.h"
#include "esp_log.h"
LV_IMAGE_DECLARE(img_app_sensor);
using namespace app_ui;
namespace esp_brookesia::apps {
Sensor *Sensor::instance_;
Sensor *Sensor::requestInstance() { if(!instance_) instance_=new(std::nothrow) Sensor; return instance_; }
Sensor::Sensor():App("Sensor",&img_app_sensor,true,false,false) {}
Sensor::~Sensor() { close(); instance_=nullptr; }
bool Sensor::run() {
    root_=screen("Sensors",event,this);
    auto *rtc=box(root_,56,88,248,78);
    label(rtc,"PCF85063  /  RTC",14,8,220,&lv_font_montserrat_12,MUTED);
    clock_=label(rtc,"--:--:--",14,26,220,&lv_font_montserrat_32);
    date_=label(rtc,"--",14,61,220,&lv_font_montserrat_12,MUTED);
    auto *imu=box(root_,56,176,248,112);
    label(imu,"QMI8658  /  MOTION",14,8,220,&lv_font_montserrat_12,MUTED);
    label(imu,"Accel (g)",14,30,110,&lv_font_montserrat_14,BLUE);
    label(imu,"Gyro (deg/s)",130,30,110,&lv_font_montserrat_14,0xd07a25);
    for(int i=0;i<6;++i) values_[i]=label(imu,"--",14+(i/3)*116,52+(i%3)*18,110,&lv_font_montserrat_14);
    status_=label(root_,"Reading sensors...",90,306,180,&lv_font_montserrat_12,MUTED);
    lv_label_set_long_mode(status_,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(status_,LV_TEXT_ALIGN_CENTER,0);
    timer_=lv_timer_create(poll,100,this);
    if(!timer_ || !start("sensor",4096)) { close(); return false; }
    return true;
}
void Sensor::work() {
    int64_t next_rtc=0,next_rtc_retry=0,last_imu=0;
    esp_err_t last_rtc_error=ESP_ERR_NOT_FINISHED;
    while(!stopping()) {
        auto now=esp_timer_get_time();
        if(now>=next_rtc) {
            bsp_sensor_datetime_t t={}; auto err=bsp_rtc_get_datetime(&t);
            if(err!=ESP_OK && now>=next_rtc_retry) {
                auto init=bsp_rtc_init();
                err=init==ESP_OK?bsp_rtc_get_datetime(&t):init;
                next_rtc_retry=esp_timer_get_time()+5000000;
            }
            if(err!=last_rtc_error) {
                if(err==ESP_OK) ESP_LOGI("sensor","Hardware RTC reading available");
                else ESP_LOGW("sensor","Hardware RTC unavailable: %s",esp_err_to_name(err));
                last_rtc_error=err;
            }
            portENTER_CRITICAL(&lock_); rtc_error_=err; if(err==ESP_OK) time_=t; portEXIT_CRITICAL(&lock_);
            next_rtc=now+1000000;
        }
        bsp_imu_data_t m={}; auto err=bsp_imu_get_data(&m);
        if(err==ESP_OK) last_imu=now;
        if(err!=ESP_ERR_NOT_FINISHED || now-last_imu>1000000) {
            portENTER_CRITICAL(&lock_); imu_error_=err; if(err==ESP_OK) motion_=m; portEXIT_CRITICAL(&lock_);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void Sensor::refresh() {
    bsp_sensor_datetime_t t; bsp_imu_data_t m; esp_err_t rtc,imu;
    portENTER_CRITICAL(&lock_); t=time_; m=motion_; rtc=rtc_error_; imu=imu_error_; portEXIT_CRITICAL(&lock_);
    char s[80];
    if(rtc==ESP_OK) {
        snprintf(s,sizeof(s),"%02u:%02u:%02u",t.hour,t.minute,t.second); text(clock_,s);
        snprintf(s,sizeof(s),"%04u-%02u-%02u",t.year,t.month,t.day); text(date_,s);
    } else { text(clock_,"--:--:--"); text(date_,"RTC unavailable"); }
    float data[]={m.accel_x,m.accel_y,m.accel_z,m.gyro_x,m.gyro_y,m.gyro_z};
    for(int i=0;i<6;++i) {
        if(imu==ESP_OK) snprintf(s,sizeof(s),"%c  %+.2f","XYZ"[i%3],(double)data[i]);
        else snprintf(s,sizeof(s),"%c  --","XYZ"[i%3]);
        text(values_[i],s);
    }
    snprintf(s,sizeof(s),"RTC: %s   IMU: %s",rtc==ESP_OK?"OK":"unavailable",imu==ESP_OK?"OK":"unavailable"); text(status_,s);
}
void Sensor::poll(lv_timer_t *t) { static_cast<Sensor *>(lv_timer_get_user_data(t))->refresh(); }
void Sensor::event(lv_event_t *e) { static_cast<Sensor *>(lv_event_get_user_data(e))->back(); }
bool Sensor::pause() { if(timer_) lv_timer_pause(timer_); stop(); return true; }
bool Sensor::resume() { if(timer_) lv_timer_resume(timer_); return start("sensor",4096); }
bool Sensor::close() { stop(); if(timer_) { lv_timer_delete(timer_); timer_=nullptr; } if(root_) lv_obj_delete(root_); root_=nullptr; return true; }
}
