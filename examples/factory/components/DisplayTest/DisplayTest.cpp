/* SPDX-License-Identifier: Apache-2.0 */
#include "DisplayTest.hpp"
#include "esp_timer.h"
#include "esp_log.h"
#include <cstdlib>
#include <new>
LV_IMAGE_DECLARE(img_app_display_test);
namespace {
constexpr uint32_t COLORS[]={0xff0000,0x00ff00,0x0000ff,0xffff00,0xffffff};
esp_brookesia::systems::phone::App::Config fullscreen_config()
{
    auto cfg=esp_brookesia::systems::phone::App::Config::SIMPLE_CONSTRUCTOR(&img_app_display_test,false,false);
    cfg.flags.enable_navigation_gesture=false;
    return cfg;
}
}
namespace esp_brookesia::apps {
DisplayTest *DisplayTest::instance_;
DisplayTest *DisplayTest::requestInstance() {
    if(!instance_) instance_=new(std::nothrow) DisplayTest;
    return instance_;
}
DisplayTest::DisplayTest():
    App(systems::base::App::Config::SIMPLE_CONSTRUCTOR("Display Test",&img_app_display_test,true),fullscreen_config()) {}
bool DisplayTest::run() {
    root_=app_ui::box(lv_screen_active(),0,0,BSP_LCD_H_RES,BSP_LCD_V_RES,COLORS[0],0);
    lv_obj_add_flag(root_,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_remove_flag(root_,LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(root_,touch,LV_EVENT_ALL,this);
    color_index_=0; last_tap_=0; exit_requested_=false;
    button_presses_=bsp_user_button_get_press_count();
    timer_=lv_timer_create(cycle,1000,this);
    exit_timer_=lv_timer_create(poll,30,this);
    if(!timer_ || !exit_timer_) { close(); return false; }
    ESP_LOGI("display_test","RGB/yellow/white, one second each; double tap or BOOT to exit");
    return true;
}
void DisplayTest::cycle(lv_timer_t *timer) {
    auto *app=static_cast<DisplayTest *>(lv_timer_get_user_data(timer));
    app->color_index_=(app->color_index_+1)%5;
    lv_obj_set_style_bg_color(app->root_,lv_color_hex(COLORS[app->color_index_]),0);
}
void DisplayTest::touch(lv_event_t *event) {
    auto *app=static_cast<DisplayTest *>(lv_event_get_user_data(event));
    auto *input=lv_indev_active(); if(!input) return;
    lv_point_t point; lv_indev_get_point(input,&point);
    const auto code=lv_event_get_code(event);
    const int64_t now=esp_timer_get_time();
    if(code==LV_EVENT_PRESSED) { app->pressed_at_=now; app->start_=point; app->moved_=false; }
    if(code==LV_EVENT_PRESSING &&
       (std::abs(point.x-app->start_.x)>20 || std::abs(point.y-app->start_.y)>20)) app->moved_=true;
    if(code==LV_EVENT_PRESS_LOST) app->last_tap_=0;
    if(code!=LV_EVENT_RELEASED) return;
    if(app->moved_ || now-app->pressed_at_>250000) { app->last_tap_=0; return; }
    if(app->last_tap_ && now-app->last_tap_<=350000 &&
       std::abs(point.x-app->previous_tap_.x)<=40 && std::abs(point.y-app->previous_tap_.y)<=40) {
        app->exit_requested_=true; app->last_tap_=0;
    } else { app->last_tap_=now; app->previous_tap_=point; }
}
void DisplayTest::poll(lv_timer_t *timer) {
    auto *app=static_cast<DisplayTest *>(lv_timer_get_user_data(timer));
    if(app->exit_requested_ || app->button_presses_!=bsp_user_button_get_press_count()) {
        lv_timer_pause(timer); app->back();
    }
}
bool DisplayTest::pause() {
    if(timer_) lv_timer_pause(timer_);
    if(exit_timer_) lv_timer_pause(exit_timer_);
    last_tap_=0; return true;
}
bool DisplayTest::resume() {
    button_presses_=bsp_user_button_get_press_count(); last_tap_=0; exit_requested_=false;
    if(timer_) lv_timer_resume(timer_);
    if(exit_timer_) lv_timer_resume(exit_timer_);
    return true;
}
bool DisplayTest::close() {
    if(timer_) { lv_timer_delete(timer_); timer_=nullptr; }
    if(exit_timer_) { lv_timer_delete(exit_timer_); exit_timer_=nullptr; }
    if(root_) { lv_obj_delete(root_); root_=nullptr; }
    return true;
}
}
