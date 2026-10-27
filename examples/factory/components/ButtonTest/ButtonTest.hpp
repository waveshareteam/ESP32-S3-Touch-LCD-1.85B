/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "app_support.hpp"

namespace esp_brookesia::apps {
class ButtonTest final : public systems::phone::App, private AppWorker {
public:
    static ButtonTest *requestInstance();
    ~ButtonTest() override { close(); instance_ = nullptr; }
protected:
    bool init() override { return true; }
    bool run() override;
    bool back() override { return notifyCoreClosed(); }
    bool close() override;
    bool deinit() override { return close(); }
    bool pause() override;
    bool resume() override;
private:
    struct Reading {
        int state = -1; // -1 unavailable, 0 released, 1 pressed
        uint32_t count = 0;
    };
    struct View { Reading buttons[1]; };
    struct Card {
        lv_obj_t *panel = nullptr, *badge = nullptr, *state = nullptr;
        lv_obj_t *count = nullptr, *result = nullptr;
        int shown_state = -2;
        uint32_t shown_count = UINT32_MAX;
    };
    ButtonTest();
    static ButtonTest *instance_;
    static void event(lv_event_t *event);
    static void poll(lv_timer_t *timer);
    void work() override;
    void publish(const View &view);
    View snapshot();
    Card createCard(const char *name, const char *signal, int y);
    void refresh();
    portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    View view_;
    std::atomic<bool> reset_{false};
    lv_obj_t *root_ = nullptr, *summary_ = nullptr, *hint_ = nullptr, *progress_ = nullptr;
    lv_timer_t *timer_ = nullptr;
    Card cards_[1];
    int shown_result_ = -1;
};
} // namespace esp_brookesia::apps
