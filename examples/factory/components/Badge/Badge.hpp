#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "app_support.hpp"
#include "badge_service.hpp"

namespace esp_brookesia::apps {
class Badge : public systems::phone::App, private AppWorker {
public:
    static Badge *requestInstance();
    ~Badge() override;
protected:
    bool init() override { return true; }
    bool run() override;
    bool back() override;
    bool close() override;
    bool deinit() override { return close(); }
    bool pause() override;
    bool resume() override;
private:
    Badge();
    void work() override;
    void refresh();
    void create_welcome();
    void show_dialog();
    void close_dialog();
    void clear_controls();
    void update_controls();
    void open_editor();
    void end_editor();
    static void event(lv_event_t *);
    static void action(lv_event_t *);
    static void poll(lv_timer_t *);
    static Badge *instance_;
    SemaphoreHandle_t frame_mutex_=nullptr;
    std::atomic<int> direction_{0};
    uint16_t *display_=nullptr,*pending_=nullptr;
    bool frame_ready_=false,frame_valid_=false,frame_empty_=false;
    uint32_t frame_revision_=0,display_revision_=0;
    bool display_valid_=false;
    bool controls_dirty_=true;
    char frame_message_[128]={};
    badge::State state_;
    lv_image_dsc_t image_={};
    lv_obj_t *root_=nullptr,*image_obj_=nullptr,*message_=nullptr;
    lv_obj_t *welcome_=nullptr,*welcome_button_=nullptr,*welcome_text_=nullptr;
    lv_obj_t *dialog_=nullptr,*editor_=nullptr,*connection_=nullptr;
    lv_obj_t *counter_=nullptr,*previous_=nullptr,*next_=nullptr;
    lv_obj_t *thumbnail_=nullptr,*preview_hint_=nullptr;
    lv_obj_t *dots_[badge::SLOTS]={};
    lv_timer_t *timer_=nullptr;
    lv_point_t touch_start_={};
    int touch_distance_=0;
    bool touch_active_=false,touch_long_pressed_=false;
    bool editing_=false,portal_seen_=false;
    int64_t editor_started_=0;
};
}
