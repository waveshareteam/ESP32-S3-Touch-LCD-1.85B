#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "app_support.hpp"
#include "media_files.h"
namespace esp_brookesia::apps {
class Photos : public systems::phone::App, private AppWorker {
public:
    static Photos *requestInstance();
    ~Photos() override;
protected:
    bool init() override { return true; }
    bool run() override;
    bool back() override { return notifyCoreClosed(); }
    bool close() override;
    bool deinit() override { return close(); }
    bool pause() override;
    bool resume() override;
private:
    Photos();
    void work() override;
    bool decode(const char *,uint16_t *,char *,size_t);
    void refresh();
    static void event(lv_event_t *);
    static void poll(lv_timer_t *);
    static bool cancel(void *ctx) { return static_cast<Photos *>(ctx)->stopping(); }
    static Photos *instance_;
    struct Slot { int index=-1; uint16_t *pixels=nullptr; bool valid=false; char error[96]={}; } slots_[3];
    SemaphoreHandle_t mutex_=nullptr;
    std::atomic<int> wanted_{0};
    size_t count_=0;
    char message_[128]={};
    int shown_=-1;
    lv_point_t touch_start_={};
    uint16_t *display_pixels_=nullptr;
    lv_image_dsc_t image_={};
    lv_obj_t *root_=nullptr,*image_obj_=nullptr,*message_obj_=nullptr;
    lv_timer_t *timer_=nullptr;
};
}
