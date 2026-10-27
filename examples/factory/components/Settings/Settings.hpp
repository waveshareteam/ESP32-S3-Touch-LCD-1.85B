#pragma once
#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {
class Settings : public systems::phone::App {
public:
    static Settings *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);
    ~Settings() override;
protected:
    Settings(bool status_bar, bool navigation_bar);
    bool init() override;
    bool run() override;
    bool back() override;
    bool close() override;
    bool deinit() override;
    bool pause() override;
    bool resume() override;
private:
    enum Page { HOME, WIFI, SD, ABOUT, PROVISION };
    static Settings *instance_;
    static void event(lv_event_t *event);
    static void poll(lv_timer_t *timer);
    void show(Page page);
    void refresh();
    void commit_controls();
    Page page_ = HOME;
    lv_obj_t *root_ = nullptr, *body_ = nullptr, *title_ = nullptr, *scroll_hint_ = nullptr;
    lv_obj_t *sliders_[2] = {}, *percent_[2] = {}, *notes_[2] = {};
    lv_obj_t *values_[6] = {}, *scan_names_[4] = {}, *scan_details_[4] = {}, *scan_rows_[4] = {};
    lv_obj_t *scan_button_ = nullptr, *previous_ = nullptr, *next_ = nullptr;
    lv_obj_t *qr_ = nullptr, *qr_hint_ = nullptr;
    bool portal_requested_ = false, qr_valid_ = false;
    char qr_ssid_[33] = {};
    lv_timer_t *timer_ = nullptr;
    bool dirty_[2] = {};
    int current_[2] = {};
    unsigned scan_page_ = 0;
};
}
