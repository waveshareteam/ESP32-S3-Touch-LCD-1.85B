#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "app_support.hpp"

namespace esp_brookesia::apps {
class Battery final : public systems::phone::App {
public:
    static Battery *requestInstance();
    ~Battery() override { close(); instance_ = nullptr; }
protected:
    Battery();
    bool init() override { return true; }
    bool run() override;
    bool back() override { return notifyCoreClosed(); }
    bool close() override;
    bool deinit() override { return close(); }
    bool pause() override;
    bool resume() override;
private:
    void refresh();
    static void event(lv_event_t *);
    static void poll(lv_timer_t *);
    static Battery *instance_;
    lv_obj_t *root_ = nullptr, *overview_ = nullptr, *details_ = nullptr;
    lv_obj_t *percent_ = nullptr, *state_ = nullptr, *bar_ = nullptr;
    lv_obj_t *voltage_ = nullptr, *current_ = nullptr;
    lv_obj_t *detail_values_[6] = {};
    lv_timer_t *timer_ = nullptr;
};
}
