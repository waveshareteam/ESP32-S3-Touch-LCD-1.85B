#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "app_support.hpp"
#include "music_file.h"
namespace esp_brookesia::apps {
class Music : public systems::phone::App, private AppWorker {
public:
    static Music *requestInstance();
    ~Music() override;
protected:
    bool init() override { return true; }
    bool run() override;
    bool back() override { return notifyCoreClosed(); }
    bool close() override;
    bool deinit() override { return close(); }
    bool pause() override;
    bool resume() override;
private:
    Music();
    static Music *instance_;
    void work() override;
    void refresh();
    static void event(lv_event_t *);
    static void poll(lv_timer_t *);
    static bool cancel(void *p) { return static_cast<Music *>(p)->stopping(); }
    std::atomic<unsigned> selected_{0};
    std::atomic<bool> play_{false};
    portMUX_TYPE lock_=portMUX_INITIALIZER_UNLOCKED;
    struct View {
        unsigned count=0,index=0;
        uint32_t position=0,duration=0;
        bool ready=false,playing=false;
        char title[128]="Local music",status[128]="Reading SD card...";
    } view_;
    void publish(const View &v) { portENTER_CRITICAL(&lock_); view_=v; portEXIT_CRITICAL(&lock_); }
    lv_obj_t *root_=nullptr,*title_=nullptr,*state_=nullptr,*count_=nullptr,*progress_=nullptr,*elapsed_=nullptr,*duration_=nullptr,*play_label_=nullptr;
    lv_timer_t *timer_=nullptr;
};
}
