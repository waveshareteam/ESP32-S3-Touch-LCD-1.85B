#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "game_2048.h"
namespace esp_brookesia::apps {
class Games : public systems::phone::App {
public:
    static Games *requestInstance();
    ~Games() override;
protected:
    bool init() override { return true; }
    bool run() override;
    bool back() override { return notifyCoreClosed(); }
    bool close() override;
    bool deinit() override { return close(); }
    bool pause() override { return true; }
    bool resume() override { return true; }
private:
    Games();
    static Games *instance_;
    static void event(lv_event_t *);
    static void gesture(lv_event_t *);
    void reset();
    void refresh();
    game_2048_state_t game_={};
    lv_obj_t *root_=nullptr,*score_=nullptr,*best_=nullptr,*hint_=nullptr,*cells_[16]={},*numbers_[16]={};
};
}
