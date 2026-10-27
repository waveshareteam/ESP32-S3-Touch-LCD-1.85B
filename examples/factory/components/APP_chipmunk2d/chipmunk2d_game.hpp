#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "bsp/esp-bsp.h"

namespace esp_brookesia::apps {
class PhoneChipmunk2d : public systems::phone::App {
public:
    static PhoneChipmunk2d *requestInstance(bool use_status_bar=false, bool use_navigation_bar=false);
    ~PhoneChipmunk2d() override { close(); }
protected:
    PhoneChipmunk2d(bool status,bool navigation);
    bool init() override;
    bool run() override;
    bool back() override { return notifyCoreClosed(); }
    bool close() override;
    bool deinit() override { return close(); }
    bool pause() override;
    bool resume() override;
private:
    static PhoneChipmunk2d *_instance;
    static void tick(lv_timer_t *);
    static void event(lv_event_t *);
    void beginCalibration();
    lv_obj_t *root_=nullptr,*heading_=nullptr,*progress_=nullptr,*hint_=nullptr;
    lv_timer_t *timer_=nullptr;
    qmi8658_dev_t *imu_=nullptr;
    unsigned samples_=0,attempts_=0;
    uint32_t boot_count_=0;
    float min_x_=0,max_x_=0,min_y_=0,max_y_=0;
    bool playing_=false, waiting_=false;
};
}
