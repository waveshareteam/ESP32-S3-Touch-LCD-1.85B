#include "Battery.hpp"
#include "battery_service.hpp"
#include "esp_timer.h"
#include <cstdio>
#include <new>
LV_IMAGE_DECLARE(img_app_battery);

using namespace app_ui;
namespace esp_brookesia::apps {
Battery *Battery::instance_;
Battery *Battery::requestInstance()
{
    if (!instance_) instance_ = new(std::nothrow) Battery;
    return instance_;
}
Battery::Battery(): App("Battery", &img_app_battery, true, false, false) {}

bool Battery::run()
{
    root_ = screen("Battery", event, this);
    overview_ = box(root_, 56, 88, 248, 204);
    label(overview_, "BQ27220", 14, 10, 220, &lv_font_montserrat_12, MUTED);
    percent_ = label(overview_, "--%", 14, 30, 220, &lv_font_montserrat_32);
    state_ = label(overview_, "Reading gauge...", 14, 72, 220, &lv_font_montserrat_14);
    bar_ = lv_bar_create(overview_);
    lv_obj_set_pos(bar_, 14, 100); lv_obj_set_size(bar_, 220, 12);
    lv_bar_set_range(bar_, 0, 100);
    lv_obj_set_style_bg_color(bar_, lv_color_hex(0xe5ecf8), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_, lv_color_hex(0x28a878), LV_PART_INDICATOR);
    label(overview_, "Voltage", 14, 128, 100, &lv_font_montserrat_12, MUTED);
    label(overview_, "Current", 130, 128, 104, &lv_font_montserrat_12, MUTED);
    voltage_ = label(overview_, "-- V", 14, 149, 108, &lv_font_montserrat_20);
    current_ = label(overview_, "-- mA", 130, 149, 108, &lv_font_montserrat_16);
    label(overview_, "+ charge / - discharge", 14, 180, 220, &lv_font_montserrat_12, MUTED);

    details_ = box(root_, 56, 88, 248, 204);
    const char *names[] = {"Remaining", "Full charge", "Design", "Avg current", "Temperature", "Cycles"};
    for (unsigned i = 0; i < 6; ++i) {
        label(details_, names[i], 12, 12 + i * 31, 113, &lv_font_montserrat_14, MUTED);
        detail_values_[i] = label(details_, "--", 129, 12 + i * 31, 107, &lv_font_montserrat_14);
    }
    lv_obj_add_flag(details_, LV_OBJ_FLAG_HIDDEN);
    button(root_, "Overview", 90, 300, 86, 34, 1, event, this);
    button(root_, "Details", 184, 300, 86, 34, 2, event, this);
    refresh();
    timer_ = lv_timer_create(poll, 1000, this);
    if (!timer_) { close(); return false; }
    return true;
}

void Battery::refresh()
{
    const auto sample = battery_service_snapshot();
    const auto state = battery_state(sample, esp_timer_get_time());
    const bool valid = battery_has_values(state);
    text(state_, battery_state_text(state));
    char value[48];
    if (valid) {
        std::snprintf(value, sizeof(value), "%u%%", sample.data.percent); text(percent_, value);
        std::snprintf(value, sizeof(value), "%.3f V", sample.data.voltage_mv / 1000.0); text(voltage_, value);
        std::snprintf(value, sizeof(value), "%+d mA", sample.data.current_ma); text(current_, value);
        lv_obj_remove_flag(bar_, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(bar_, sample.data.percent, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar_, lv_color_hex(sample.data.percent <= 15 ? 0xe05252 : 0x28a878), LV_PART_INDICATOR);
    } else {
        text(percent_, "--%"); text(voltage_, "-- V"); text(current_, "-- mA");
        lv_obj_add_flag(bar_, LV_OBJ_FLAG_HIDDEN);
    }
    const unsigned capacities[] = {sample.data.remaining_mah, sample.data.full_mah, sample.data.design_mah};
    for (unsigned i = 0; i < 6; ++i) {
        if (!valid) std::snprintf(value, sizeof(value), "--");
        else if (i < 3) std::snprintf(value, sizeof(value), "%u mAh", capacities[i]);
        else if (i == 3) std::snprintf(value, sizeof(value), "%+d mA", sample.data.average_current_ma);
        else if (i == 4) std::snprintf(value, sizeof(value), "%.1f C", double(sample.data.temperature_c));
        else std::snprintf(value, sizeof(value), "%u", sample.data.cycles);
        text(detail_values_[i], value);
    }
}

void Battery::event(lv_event_t *e)
{
    auto *self = static_cast<Battery *>(lv_event_get_user_data(e));
    const auto id = action(e);
    if (id == 0) { self->back(); return; }
    if (id == 1) { lv_obj_remove_flag(self->overview_, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(self->details_, LV_OBJ_FLAG_HIDDEN); }
    if (id == 2) { lv_obj_add_flag(self->overview_, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(self->details_, LV_OBJ_FLAG_HIDDEN); }
    self->refresh();
}
void Battery::poll(lv_timer_t *timer) { static_cast<Battery *>(lv_timer_get_user_data(timer))->refresh(); }
bool Battery::pause() { if (timer_) lv_timer_pause(timer_); return true; }
bool Battery::resume() { refresh(); if (timer_) lv_timer_resume(timer_); return true; }
bool Battery::close()
{
    if (timer_) { lv_timer_delete(timer_); timer_ = nullptr; }
    if (root_) lv_obj_delete(root_);
    root_ = nullptr;
    return true;
}
}
