#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"
namespace esp_brookesia::apps {
class Weather : public systems::phone::App {
public:
    static Weather *requestInstance();
    ~Weather() override;
protected:
    bool init() override;
    bool run() override;
    bool back() override { return notifyCoreClosed(); }
    bool close() override;
    bool deinit() override { return close(); }
    bool pause() override;
    bool resume() override;
private:
    Weather();
    static Weather *instance_;
    static void event(lv_event_t *);
    static void poll(lv_timer_t *);
    void refresh();
    lv_obj_t *root_=nullptr,*city_=nullptr,*date_=nullptr,*temp_=nullptr,*condition_=nullptr,*humidity_=nullptr,*wind_=nullptr,*message_=nullptr;
    lv_timer_t *timer_=nullptr;
    lv_obj_t *icon_=nullptr,*direction_=nullptr,*status_=nullptr,*status_dot_=nullptr,*refresh_button_=nullptr;
    int icon_condition_=-1;
    uint32_t status_color_=0;
    bool service_ready_=false;
};
}
