/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "app_support.hpp"
namespace esp_brookesia::apps {
class DisplayTest final : public systems::phone::App {
public:
    static DisplayTest *requestInstance();
    ~DisplayTest() override { close(); instance_=nullptr; }
protected:
    DisplayTest();
    bool init() override { return true; }
    bool run() override;
    bool back() override { return notifyCoreClosed(); }
    bool close() override;
    bool deinit() override { return close(); }
    bool pause() override;
    bool resume() override;
private:
    static DisplayTest *instance_;
    static void cycle(lv_timer_t *);
    static void touch(lv_event_t *);
    static void poll(lv_timer_t *);
    lv_obj_t *root_=nullptr;
    lv_timer_t *timer_=nullptr, *exit_timer_=nullptr;
    unsigned color_index_=0;
    uint32_t button_presses_=0;
    int64_t pressed_at_=0, last_tap_=0;
    lv_point_t start_={}, previous_tap_={};
    bool moved_=false, exit_requested_=false;
};
}
