#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "lvgl.h"
#include "wifi_manager.h"
#include "watch_angles.h"

namespace esp_brookesia::apps {
class Watch : public systems::phone::App {
public:
    static Watch *requestInstance();
    ~Watch() override;
protected:
    bool init() override { return true; }
    bool run() override;
    bool back() override;
    bool close() override;
    bool deinit() override { return close(); }
    bool pause() override;
    bool resume() override;
private:
    Watch();
    void refresh();
    void apply_mode();
    static void draw_dial(lv_event_t *);
    static void draw_hands(lv_event_t *);
    static void touch(lv_event_t *);
    static void tick(lv_timer_t *);
    static Watch *instance_;
    lv_obj_t *root_=nullptr,*dial_=nullptr,*hands_=nullptr;
    lv_obj_t *time_=nullptr,*date_=nullptr,*source_=nullptr;
    lv_obj_t *waiting_=nullptr,*help_=nullptr;
    lv_timer_t *timer_=nullptr;
    watch_angles_t angles_={};
    lv_point_t touch_start_={};
    uint32_t button_presses_=0;
    int mode_=0;
    bool valid_=false,touch_active_=false,swiped_=false,closing_=false;
    time_t last_second_=(time_t)-1;
    wifi_clock_source_t last_source_=WIFI_CLOCK_UNSET;
};
}
