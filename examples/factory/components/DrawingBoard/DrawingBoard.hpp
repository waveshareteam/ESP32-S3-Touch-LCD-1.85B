#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {
class DrawingBoard : public systems::phone::App {
public:
    static DrawingBoard *requestInstance();
    ~DrawingBoard() override;
protected:
    DrawingBoard();
    bool init() override;
    bool run() override;
    bool back() override;
    bool close() override;
    bool deinit() override;
    bool pause() override;
    bool resume() override;
private:
    static DrawingBoard *instance_;
    static void touch_event(lv_event_t *event);
    static void back_event(lv_event_t *event);
    static void poll_exit(lv_timer_t *timer);
    void clear();
    void draw(const lv_point_t &point);
    lv_obj_t *root_ = nullptr, *canvas_ = nullptr;
    lv_timer_t *exit_timer_ = nullptr;
    uint32_t button_presses_ = 0;
    uint8_t *pixels_ = nullptr;
    lv_draw_buf_t draw_buffer_ = {};
    lv_point_t previous_ = {};
    bool stroke_ = false;
};
}
