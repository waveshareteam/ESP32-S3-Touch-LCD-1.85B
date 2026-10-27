// Round-screen Settings: all main entries fit the visible home page.
#include "Settings.hpp"
#include "settings_service.h"
#include "wifi_manager.h"
#include <cstdio>
#include <cstring>
#include <new>

LV_IMAGE_DECLARE(img_app_settings);

namespace {
constexpr uint32_t BACKGROUND = 0xf3f6fb, INK = 0x1d2b42, MUTED = 0x78859a, ACCENT = 0x376df6;
constexpr int CONTENT_W = 248;
enum Action { BACK = 1, OPEN_WIFI, OPEN_SD, OPEN_ABOUT, SCAN, PREVIOUS, NEXT, CHECK_SD, BRIGHTNESS, VOLUME, OPEN_SETUP };

lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width,
                const lv_font_t *font = &lv_font_montserrat_16, uint32_t color = INK)
{
    auto *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, width);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_label_set_text(obj, text);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h)
{
    auto *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 14, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                 Action action, lv_event_cb_t callback, void *context, bool primary = false)
{
    auto *obj = box(parent, x, y, w, h);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(primary ? ACCENT : 0xffffff), 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(primary ? 0x2555d5 : 0xe4ebf7), LV_STATE_PRESSED);
    lv_obj_set_style_opa(obj, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_set_user_data(obj, reinterpret_cast<void *>(action));
    lv_obj_add_event_cb(obj, callback, LV_EVENT_CLICKED, context);
    if (text) {
        auto *caption = label(obj, text, 0, 0, w, &lv_font_montserrat_16, primary ? 0xffffff : INK);
        lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(caption);
    }
    return obj;
}

void set_text(lv_obj_t *obj, const char *text)
{
    if (obj && std::strcmp(lv_label_get_text(obj), text)) lv_label_set_text(obj, text);
}
void enabled(lv_obj_t *obj, bool value)
{
    if (value) lv_obj_remove_state(obj, LV_STATE_DISABLED);
    else lv_obj_add_state(obj, LV_STATE_DISABLED);
}
}

namespace esp_brookesia::apps {
Settings *Settings::instance_;
Settings *Settings::requestInstance(bool status, bool navigation)
{
    if (!instance_) instance_ = new (std::nothrow) Settings(status, navigation);
    return instance_;
}
Settings::Settings(bool status, bool navigation) : App("Settings", &img_app_settings, true, status, navigation) {}
Settings::~Settings() { instance_ = nullptr; }
bool Settings::init() { return settings_service_start(); }

bool Settings::run()
{
    const auto area = getVisualArea();
    root_ = box(lv_screen_active(), area.x1, area.y1, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_hex(BACKGROUND), 0);
    button(root_, LV_SYMBOL_LEFT, 76, 40, 36, 36, BACK, event, this);
    title_ = label(root_, "Settings", 118, 46, 166, &lv_font_montserrat_20);
    body_ = box(root_, (BSP_LCD_H_RES-CONTENT_W)/2, 88, CONTENT_W, 210);
    lv_obj_set_style_bg_opa(body_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(body_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(body_, LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scrollbar_mode(body_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_scroll_dir(body_, LV_DIR_VER);
    lv_obj_set_style_pad_bottom(body_, 12, 0);
    scroll_hint_ = label(root_, "Scroll for details", 108, 316, 144, &lv_font_montserrat_12, MUTED);
    lv_obj_set_style_text_align(scroll_hint_, LV_TEXT_ALIGN_CENTER, 0);
    show(HOME);
    timer_ = lv_timer_create(poll, 150, this);
    if (!timer_) { close(); return false; }
    return true;
}

void Settings::commit_controls()
{
    if (dirty_[0]) settings_service_set_brightness(current_[0], true);
    if (dirty_[1]) settings_service_set_volume(current_[1], true);
    dirty_[0] = dirty_[1] = false;
}

void Settings::show(Page page)
{
    commit_controls();
    if(page_==PROVISION && page!=PROVISION) wifi_manager_stop_provisioning();
    lv_obj_clean(body_);
    lv_obj_scroll_to_y(body_, 0, LV_ANIM_OFF);
    page_ = page;
    if (page == HOME) lv_obj_add_flag(scroll_hint_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(scroll_hint_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_pad_bottom(body_, page == HOME ? 0 : 12, 0);
    qr_ = qr_hint_ = nullptr;
    portal_requested_ = qr_valid_ = false;
    qr_ssid_[0] = '\0';
    std::memset(sliders_, 0, sizeof(sliders_));
    std::memset(percent_, 0, sizeof(percent_));
    std::memset(notes_, 0, sizeof(notes_));
    std::memset(values_, 0, sizeof(values_));
    if (page == HOME) {
        set_text(title_, "Settings");
        const char *names[] = {"Brightness", "Volume"};
        const char *icons[] = {LV_SYMBOL_IMAGE, LV_SYMBOL_VOLUME_MAX};
        for (int i = 0; i < 2; ++i) {
            auto *card = box(body_, 0, i * 64, CONTENT_W, 56);
            label(card, icons[i], 12, 6, 22, &lv_font_montserrat_14, ACCENT);
            label(card, names[i], 40, 6, 136, &lv_font_montserrat_14);
            percent_[i] = label(card, "", 180, 6, 54, &lv_font_montserrat_14);
            lv_obj_set_style_text_align(percent_[i], LV_TEXT_ALIGN_RIGHT, 0);
            sliders_[i] = lv_slider_create(card);
            lv_obj_set_pos(sliders_[i], 22, 32); lv_obj_set_size(sliders_[i], 204, 5);
            lv_slider_set_range(sliders_[i], i == 0 ? 10 : 0, 100);
            lv_obj_set_style_bg_color(sliders_[i], lv_color_hex(0xe2e9f5), LV_PART_MAIN);
            lv_obj_set_style_bg_color(sliders_[i], lv_color_hex(ACCENT), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(sliders_[i], lv_color_hex(ACCENT), LV_PART_KNOB);
            lv_obj_set_style_pad_all(sliders_[i], 7, LV_PART_KNOB);
            lv_obj_set_ext_click_area(sliders_[i], 10);
            lv_obj_remove_flag(sliders_[i], LV_OBJ_FLAG_GESTURE_BUBBLE);
            lv_obj_set_user_data(sliders_[i], reinterpret_cast<void *>(i == 0 ? BRIGHTNESS : VOLUME));
            lv_obj_add_event_cb(sliders_[i], event, LV_EVENT_ALL, this);
            notes_[i] = label(card, "", 14, 43, 220, &lv_font_montserrat_12, MUTED);
            lv_label_set_long_mode(notes_[i], LV_LABEL_LONG_DOT);
        }
        enabled(sliders_[1], bsp_get_init_status()->audio == ESP_OK);
        const char *titles[] = {"Wi-Fi scan", "Connect Wi-Fi", "SD card", "Device info"};
        const Action actions[] = {OPEN_WIFI, OPEN_SETUP, OPEN_SD, OPEN_ABOUT};
        for (int i = 0; i < 4; ++i) {
            auto *tile = button(body_, nullptr, (i % 2) * 128, 128 + (i / 2) * 42,
                                120, 36, actions[i], event, this);
            auto *caption = label(tile, titles[i], 6, 0, 108, &lv_font_montserrat_14);
            lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_center(caption);
        }
    } else if (page == WIFI) {
        set_text(title_, "Wi-Fi scan");
        scan_button_ = button(body_, "Scan again", 0, 0, CONTENT_W, 40, SCAN, event, this, true);
        values_[0] = label(body_, "", 4, 51, 240, &lv_font_montserrat_14, MUTED);
        for (int i = 0; i < 4; ++i) {
            scan_rows_[i] = box(body_, 0, 74 + i * 68, CONTENT_W, 62);
            scan_names_[i] = label(scan_rows_[i], "", 12, 9, 224, &lv_font_source_han_sans_sc_16_cjk);
            lv_label_set_long_mode(scan_names_[i], LV_LABEL_LONG_DOT);
            scan_details_[i] = label(scan_rows_[i], "", 12, 37, 224, &lv_font_montserrat_12, MUTED);
        }
        previous_ = button(body_, LV_SYMBOL_LEFT, 0, 354, 52, 36, PREVIOUS, event, this);
        next_ = button(body_, LV_SYMBOL_RIGHT, 196, 354, 52, 36, NEXT, event, this);
        values_[1] = label(body_, "", 58, 364, 132, &lv_font_montserrat_14, MUTED);
        lv_obj_set_style_text_align(values_[1], LV_TEXT_ALIGN_CENTER, 0);
    } else if (page == PROVISION) {
        set_text(title_, "Wi-Fi setup");
        auto *intro = label(body_, "Scan to join the device hotspot", 0, 0, CONTENT_W, &lv_font_montserrat_14);
        lv_obj_set_style_text_align(intro, LV_TEXT_ALIGN_CENTER, 0);
        auto *qr_card = box(body_, 0, 28, CONTENT_W, 168);
        qr_ = lv_qrcode_create(qr_card);
        if (qr_) {
            lv_qrcode_set_size(qr_, 160);
            lv_qrcode_set_dark_color(qr_, lv_color_black());
            lv_qrcode_set_light_color(qr_, lv_color_white());
            lv_qrcode_set_quiet_zone(qr_, true);
            lv_obj_center(qr_);
            lv_obj_remove_flag(qr_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(qr_, LV_OBJ_FLAG_HIDDEN);
        }
        qr_hint_ = label(qr_card, "Starting hotspot...", 12, 0, 224, &lv_font_montserrat_14, MUTED);
        lv_obj_set_style_text_align(qr_hint_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(qr_hint_);
        auto *card=box(body_,0,224,CONTENT_W,92);
        values_[0]=label(card,"",12,14,224,&lv_font_montserrat_16);
        label(card,"Password: " WIFI_PROVISION_PASSWORD,12,40,224,&lv_font_montserrat_14,MUTED);
        label(card,WIFI_PROVISION_URL,12,64,224,&lv_font_montserrat_14,ACCENT);
        label(body_,"After joining, open 192.168.4.1\nto enter your 2.4 GHz Wi-Fi details.",8,324,232,&lv_font_montserrat_12,MUTED);
        values_[1]=label(body_,"",8,360,232,&lv_font_montserrat_12,ACCENT);
    } else if (page == SD) {
        set_text(title_, "SD card");
        auto *top = box(body_, 0, 0, CONTENT_W, 90);
        label(top, LV_SYMBOL_SD_CARD, 18, 20, 40, &lv_font_montserrat_32, ACCENT);
        values_[0] = label(top, "", 72, 20, 164, &lv_font_montserrat_20);
        values_[1] = label(top, "", 72, 51, 164, &lv_font_montserrat_12, MUTED);
        const char *names[] = {"Card capacity", "Filesystem size", "Free space"};
        for (int i = 0; i < 3; ++i) {
            auto *row = box(body_, 0, 106 + i * 64, CONTENT_W, 54);
            label(row, names[i], 14, 18, 120, &lv_font_montserrat_14, MUTED);
            values_[i + 2] = label(row, "--", 134, 17, 100);
            lv_obj_set_style_text_align(values_[i + 2], LV_TEXT_ALIGN_RIGHT, 0);
        }
        label(body_, "GB = 1,000,000,000 bytes", 8, 310, 232, &lv_font_montserrat_12, MUTED);
        button(body_, "Refresh", 0, 346, CONTENT_W, 44, CHECK_SD, event, this, true);
    } else {
        set_text(title_, "About");
        label(body_, "ESP32-S3\nTouch LCD 1.85B", 8, 4, 232, &lv_font_montserrat_24);
        settings_snapshot_t snapshot;
        settings_service_snapshot(&snapshot);
        char flash[24], psram[24];
        std::snprintf(flash, sizeof(flash), "%lu MB", (unsigned long)snapshot.flash_mb);
        std::snprintf(psram, sizeof(psram), "%lu MB", (unsigned long)snapshot.psram_mb);
        const char *names[] = {"Firmware", "ESP-IDF", "Display", "Flash", "PSRAM", "MAC"};
        const char *texts[] = {snapshot.firmware, snapshot.sdk, "360 x 360", flash, psram, snapshot.mac};
        for (int i = 0; i < 6; ++i) {
            auto *row = box(body_, 0, 84 + i * 50, CONTENT_W, 44);
            label(row, names[i], 12, 14, 76, &lv_font_montserrat_14, MUTED);
            auto *value = label(row, texts[i], 90, 14, 146, &lv_font_montserrat_14);
            lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
        }
    }
    lv_obj_update_layout(body_);
    lv_obj_scroll_to_y(body_, 0, LV_ANIM_OFF);
    refresh();
}

void Settings::refresh()
{
    if (!body_) return;
    settings_snapshot_t snapshot;
    settings_service_snapshot(&snapshot, scan_page_ * SETTINGS_SCAN_PAGE_SIZE);
    char text[96];
    if (page_ == HOME) {
        for (int i = 0; i < 2; ++i) {
            if (!dirty_[i] && !lv_slider_is_dragged(sliders_[i])) {
                current_[i] = i == 0 ? snapshot.brightness : snapshot.volume;
                lv_slider_set_value(sliders_[i], current_[i], LV_ANIM_OFF);
            }
            std::snprintf(text, sizeof(text), "%d%%", current_[i]);
            set_text(percent_[i], text);
            const char *note = snapshot.controls_pending ? "Applying..." : "";
            if (snapshot.controls_error != ESP_OK) note = "Could not apply setting";
            if (snapshot.save_error != ESP_OK) note = "Could not save setting";
            if (i == 1 && bsp_get_init_status()->audio != ESP_OK) note = "Audio unavailable";
            set_text(notes_[i], note);
        }
    } else if (page_ == WIFI) {
        unsigned pages = (snapshot.scan_count + SETTINGS_SCAN_PAGE_SIZE - 1) / SETTINGS_SCAN_PAGE_SIZE;
        if (scan_page_ >= pages && scan_page_) {
            scan_page_ = 0;
            settings_service_snapshot(&snapshot, 0);
        }
        if (snapshot.scan_busy) std::snprintf(text, sizeof(text), "Scanning nearby networks...");
        else if (snapshot.scan_error != ESP_OK) std::snprintf(text, sizeof(text), "Scan failed: %s", esp_err_to_name(snapshot.scan_error));
        else if (!snapshot.scan_count) std::snprintf(text, sizeof(text), "No networks found");
        else std::snprintf(text, sizeof(text), "%u networks  /  Scan only", snapshot.scan_count);
        set_text(values_[0], text);
        enabled(scan_button_, !snapshot.scan_busy);
        enabled(previous_, scan_page_ > 0);
        enabled(next_, scan_page_ + 1 < pages);
        for (unsigned i = 0; i < 4; ++i) {
            if (i >= snapshot.scan_page_count) { lv_obj_add_flag(scan_rows_[i], LV_OBJ_FLAG_HIDDEN); continue; }
            lv_obj_remove_flag(scan_rows_[i], LV_OBJ_FLAG_HIDDEN);
            const auto &ap = snapshot.networks[i];
            set_text(scan_names_[i], ap.ssid[0] ? ap.ssid : "<Hidden SSID>");
            std::snprintf(text, sizeof(text), "%s   %d dBm   CH %u", ap.security, ap.rssi, ap.channel);
            set_text(scan_details_[i], text);
        }
        std::snprintf(text, sizeof(text), "%u / %u", pages ? scan_page_ + 1 : 0, pages);
        set_text(values_[1], text);
    } else if (page_ == PROVISION) {
        wifi_manager_snapshot_t net={}; wifi_manager_get_snapshot(&net);
        // Retry only until the worker accepts this visit's request. Never reopen
        // the hotspot automatically after successful provisioning or an error.
        if (!portal_requested_ && net.ready) {
            portal_requested_ = wifi_manager_start_provisioning();
            wifi_manager_get_snapshot(&net);
            if (portal_requested_) std::snprintf(net.message, sizeof(net.message), "Starting hotspot...");
        }
        set_text(values_[0],net.ap_ssid[0]?net.ap_ssid:"Starting Wi-Fi...");
        const char *status = net.message[0] ? net.message : "Waiting for Wi-Fi...";
        const char *hint = status;
        const bool provisioned = net.provisioning_success && net.connected;
        if (provisioned) {
            hint = "Connected!\nYou can go back.";
            status = net.credentials_saved ? "Connected and saved." : "Connected; could not save Wi-Fi.";
        } else if (net.provisioning) {
            if (qr_ && net.ap_ssid[0] && std::strcmp(qr_ssid_, net.ap_ssid)) {
                // The generated AP name/password contain no QR-reserved characters.
                char payload[128];
                int length = std::snprintf(payload, sizeof(payload), "WIFI:T:WPA;P:%s;S:%s;H:false;;",
                                           WIFI_PROVISION_PASSWORD, net.ap_ssid);
                qr_valid_ = length > 0 && static_cast<size_t>(length) < sizeof(payload) &&
                            lv_qrcode_update(qr_, payload, length) == LV_RESULT_OK;
                std::snprintf(qr_ssid_, sizeof(qr_ssid_), "%s", net.ap_ssid);
            }
            hint = "QR unavailable.\nJoin the hotspot manually.";
        }
        const bool show_qr = net.provisioning && !provisioned && qr_valid_;
        if (qr_) {
            if (show_qr) lv_obj_remove_flag(qr_, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(qr_, LV_OBJ_FLAG_HIDDEN);
        }
        if (show_qr) lv_obj_add_flag(qr_hint_, LV_OBJ_FLAG_HIDDEN);
        else {
            set_text(qr_hint_, hint);
            lv_obj_remove_flag(qr_hint_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_center(qr_hint_);
        }
        set_text(values_[1], status);
    } else if (page_ == SD) {
        set_text(values_[0], snapshot.sd_busy ? "Checking..." : snapshot.sd.mounted ? "Mounted" : "Unavailable");
        set_text(values_[1], snapshot.sd_busy ? "Please wait" : snapshot.sd_error == ESP_OK ? "/sdcard" : esp_err_to_name(snapshot.sd_error));
        const uint64_t bytes[] = {snapshot.sd.capacity_bytes, snapshot.sd.total_bytes, snapshot.sd.free_bytes};
        for (int i = 0; i < 3; ++i) {
            if (snapshot.sd.mounted) std::snprintf(text, sizeof(text), "%.2f GB", bytes[i] / 1e9);
            else std::snprintf(text, sizeof(text), "--");
            set_text(values_[i + 2], text);
        }
    }
}

void Settings::event(lv_event_t *e)
{
    auto *app = static_cast<Settings *>(lv_event_get_user_data(e));
    auto *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    Action action = static_cast<Action>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(target)));
    auto code = lv_event_get_code(e);
    if (action == BRIGHTNESS || action == VOLUME) {
        const int index = action == BRIGHTNESS ? 0 : 1;
        if (code == LV_EVENT_VALUE_CHANGED) {
            app->current_[index] = lv_slider_get_value(target);
            app->dirty_[index] = true;
            if (index == 0) settings_service_set_brightness(app->current_[index], false);
            else settings_service_set_volume(app->current_[index], false);
            app->refresh();
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST || code == LV_EVENT_DEFOCUSED) app->commit_controls();
        return;
    }
    if (code != LV_EVENT_CLICKED) return;
    switch (action) {
    case BACK: app->back(); break;
    case OPEN_WIFI: app->scan_page_ = 0; settings_service_scan(); app->show(WIFI); break;
    case OPEN_SD: settings_service_check_sd(); app->show(SD); break;
    case OPEN_ABOUT: app->show(ABOUT); break;
    case OPEN_SETUP: app->show(PROVISION); break;
    case SCAN: app->scan_page_ = 0; settings_service_scan(); app->refresh(); break;
    case PREVIOUS: if (app->scan_page_) --app->scan_page_; app->refresh(); break;
    case NEXT: ++app->scan_page_; app->refresh(); break;
    case CHECK_SD: settings_service_check_sd(); app->refresh(); break;
    default: break;
    }
}
void Settings::poll(lv_timer_t *timer) { static_cast<Settings *>(lv_timer_get_user_data(timer))->refresh(); }
bool Settings::back()
{
    if (page_ != HOME) { show(HOME); return true; }
    return notifyCoreClosed();
}
bool Settings::close()
{
    if(page_==PROVISION) wifi_manager_stop_provisioning();
    commit_controls();
    if (timer_) { lv_timer_delete(timer_); timer_ = nullptr; }
    if (root_) lv_obj_delete(root_);
    root_ = body_ = title_ = scroll_hint_ = nullptr;
    qr_ = qr_hint_ = nullptr;
    portal_requested_ = qr_valid_ = false;
    qr_ssid_[0] = '\0';
    return true;
}
bool Settings::deinit() { return close(); }
bool Settings::pause()
{
    commit_controls();
    if (page_ == PROVISION) show(HOME);
    if (timer_) lv_timer_pause(timer_);
    return true;
}
bool Settings::resume() { if (timer_) lv_timer_resume(timer_); refresh(); return true; }
}
