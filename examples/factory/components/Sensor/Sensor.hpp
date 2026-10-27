#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "app_support.hpp"
namespace esp_brookesia::apps {
class Sensor : public systems::phone::App, private AppWorker {
public:
    static Sensor *requestInstance();
    ~Sensor() override;
protected:
    bool init() override { return true; }
    bool run() override;
    bool back() override { return notifyCoreClosed(); }
    bool close() override;
    bool deinit() override { return close(); }
    bool pause() override;
    bool resume() override;
private:
    Sensor();
    void work() override;
    void refresh();
    static void event(lv_event_t *);
    static void poll(lv_timer_t *);
    static Sensor *instance_;
    lv_obj_t *root_=nullptr,*clock_=nullptr,*date_=nullptr,*status_=nullptr,*values_[6]={};
    lv_timer_t *timer_=nullptr;
    portMUX_TYPE lock_=portMUX_INITIALIZER_UNLOCKED;
    bsp_sensor_datetime_t time_={};
    bsp_imu_data_t motion_={};
    esp_err_t rtc_error_=ESP_ERR_NOT_FINISHED,imu_error_=ESP_ERR_NOT_FINISHED;
};
}
