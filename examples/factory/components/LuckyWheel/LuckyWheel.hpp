#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "lvgl.h"
#include "wheel_motion.h"

namespace esp_brookesia::apps {
class LuckyWheel : public systems::phone::App {
public:
    static LuckyWheel *requestInstance();
    ~LuckyWheel() override;
protected:
    bool init() override { return true; }
    bool run() override;
    bool back() override;
    bool close() override;
    bool deinit() override { return close(); }
    bool pause() override;
    bool resume() override;
private:
    LuckyWheel();
    void start_spin();
    void reset_result();
    void show_options();
    void hide_options(bool apply);
    void refresh_count();
    static void draw(lv_event_t *);
    static void action(lv_event_t *);
    static void touch(lv_event_t *);
    static void tick(lv_timer_t *);
    static LuckyWheel *instance_;
    lv_obj_t *root_=nullptr,*wheel_=nullptr,*hub_=nullptr,*options_=nullptr;
    lv_obj_t *caption_=nullptr,*result_=nullptr,*hint_=nullptr;
    lv_obj_t *menu_=nullptr,*count_label_=nullptr,*minus_=nullptr,*plus_=nullptr;
    lv_timer_t *timer_=nullptr;
    int count_=8,pending_count_=8;
    int32_t rotation_=0,spin_start_=0,spin_target_=0;
    int64_t started_at_=0,paused_at_=0;
    uint32_t duration_=0,button_presses_=0;
    bool spinning_=false,touch_active_=false;
    lv_point_t touch_start_={};
};
}
