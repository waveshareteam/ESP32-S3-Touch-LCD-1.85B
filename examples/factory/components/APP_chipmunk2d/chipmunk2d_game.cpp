/* SPDX-License-Identifier: Apache-2.0 */
#include "chipmunk2d_game.hpp"
#include "chipmunk_demo_1/chipmunk_demo_1.h"
#include "esp_brookesia.hpp"
#include <algorithm>

LV_IMAGE_DECLARE(img_app_qmi8658ball);
namespace esp_brookesia::apps {
PhoneChipmunk2d *PhoneChipmunk2d::_instance=nullptr;
PhoneChipmunk2d *PhoneChipmunk2d::requestInstance(bool status,bool navigation) {
    if(!_instance) _instance=new PhoneChipmunk2d(status,navigation);
    return _instance;
}
PhoneChipmunk2d::PhoneChipmunk2d(bool status,bool navigation):
    App("Chipmunk2d",&img_app_qmi8658ball,true,status,navigation) {}
bool PhoneChipmunk2d::init() { imu_=bsp_imu_get_handle(); return imu_!=nullptr; }
bool PhoneChipmunk2d::run() {
    root_=lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(root_); lv_obj_set_size(root_,360,360);
    lv_obj_set_style_bg_color(root_,lv_color_black(),0);
    lv_obj_set_style_bg_opa(root_,LV_OPA_COVER,0);
    lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    auto *back=lv_button_create(root_); lv_obj_set_pos(back,76,40); lv_obj_set_size(back,36,36);
    lv_obj_add_event_cb(back,event,LV_EVENT_CLICKED,this);
    auto *arrow=lv_label_create(back); lv_label_set_text(arrow,LV_SYMBOL_LEFT); lv_obj_center(arrow);
    heading_=lv_label_create(root_); lv_label_set_text(heading_,"Gravity");
    lv_obj_set_pos(heading_,124,46); lv_obj_set_style_text_color(heading_,lv_color_white(),0);
    lv_obj_set_style_text_font(heading_,&lv_font_montserrat_20,0);
    progress_=lv_bar_create(root_); lv_obj_set_pos(progress_,80,162); lv_obj_set_size(progress_,200,12);
    hint_=lv_label_create(root_); lv_obj_set_pos(hint_,80,192); lv_obj_set_width(hint_,200);
    lv_obj_set_style_text_align(hint_,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_color(hint_,lv_color_white(),0);
    boot_count_=bsp_user_button_get_press_count();
    beginCalibration(); timer_=lv_timer_create(tick,20,this);
    if(!timer_) { close(); return false; }
    return true;
}
void PhoneChipmunk2d::beginCalibration() {
    chipmunk_game_deinit(); playing_=waiting_=false; samples_=attempts_=0;
    lv_obj_set_style_bg_opa(root_,LV_OPA_COVER,0);
    lv_obj_remove_flag(progress_,LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(hint_,LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(progress_,0,LV_ANIM_OFF);
    lv_label_set_text(hint_,"Keep the board still");
}
void PhoneChipmunk2d::tick(lv_timer_t *timer) {
    auto *app=static_cast<PhoneChipmunk2d *>(lv_timer_get_user_data(timer));
    const uint32_t count=bsp_user_button_get_press_count();
    if(count!=app->boot_count_) { app->boot_count_=count; app->beginCalibration(); }
    if(app->playing_ || app->waiting_) return;
    qmi8658_data_t data={};
    ++app->attempts_;
    if(qmi8658_read_sensor_data(app->imu_,&data)==ESP_OK) {
        if(!app->samples_) {
            app->min_x_=app->max_x_=data.accelX; app->min_y_=app->max_y_=data.accelY;
        } else {
            app->min_x_=std::min(app->min_x_,data.accelX); app->max_x_=std::max(app->max_x_,data.accelX);
            app->min_y_=std::min(app->min_y_,data.accelY); app->max_y_=std::max(app->max_y_,data.accelY);
        }
        ++app->samples_;
        lv_bar_set_value(app->progress_,app->samples_,LV_ANIM_OFF);
    }
    if(app->samples_>=100) {
        if(app->max_x_-app->min_x_>0.1f || app->max_y_-app->min_y_>0.1f) {
            app->samples_=app->attempts_=0;
            lv_label_set_text(app->hint_,"Hold still to start");
            return;
        }
        lv_obj_add_flag(app->progress_,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(app->hint_,LV_OBJ_FLAG_HIDDEN);
        start_chipmunk_esp_example_1(app->imu_); app->playing_=true;
        // The game board is below this APP's header. Calibration and physics
        // both run under LVGL, so switching APPs cannot free UI mid-update.
        lv_obj_set_style_bg_opa(app->root_,LV_OPA_TRANSP,0);
        lv_obj_move_foreground(app->root_);
    } else if(app->attempts_>=200) {
        lv_label_set_text(app->hint_,"Sensor unavailable\nPress BOOT to retry");
        app->waiting_=true;
    }
}
void PhoneChipmunk2d::event(lv_event_t *e) {
    static_cast<PhoneChipmunk2d *>(lv_event_get_user_data(e))->back();
}
bool PhoneChipmunk2d::pause() {
    if(timer_) lv_timer_pause(timer_);
    chipmunk_game_pause(); return true;
}
bool PhoneChipmunk2d::resume() {
    if(timer_) lv_timer_resume(timer_);
    chipmunk_game_resume(); return true;
}
bool PhoneChipmunk2d::close() {
    if(timer_) { lv_timer_delete(timer_); timer_=nullptr; }
    chipmunk_game_deinit();
    if(root_) { lv_obj_delete(root_); root_=nullptr; }
    playing_=false; return true;
}
}
