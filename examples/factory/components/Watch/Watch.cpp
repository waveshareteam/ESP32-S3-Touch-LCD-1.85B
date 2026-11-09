/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 *
 * Classic watch-face interaction adapted for Brookesia and a 360 px panel.
 * Native vector artwork and the RTC/system-clock bridge are product-specific.
 */
#include "Watch.hpp"
#include "app_support.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>

LV_IMAGE_DECLARE(img_app_watch);

namespace {
constexpr int SIZE=360,CENTER=180;
// Match the ivory dial, charcoal hands and red seconds of the launcher icon.
constexpr uint32_t FACE=0xf4f0e7,INK=0x26313c,RED=0xce4854;
constexpr uint32_t RIM=0x858b91,BEZEL=0xd7d1c5,MUTED=0x7d827c;
constexpr double PI=3.14159265358979323846;

esp_brookesia::systems::phone::App::Config fullscreen_config()
{
    auto cfg=esp_brookesia::systems::phone::App::Config::SIMPLE_CONSTRUCTOR(&img_app_watch,false,false);
    cfg.flags.enable_navigation_gesture=false;
    return cfg;
}

lv_obj_t *centered(lv_obj_t *parent,const char *text,int y,const lv_font_t *font,uint32_t color)
{
    auto *label=app_ui::label(parent,text,0,y,280,font,color);
    lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_align(label,LV_ALIGN_TOP_MID,0,y);
    return label;
}

void visible(lv_obj_t *object,bool show)
{
    if (show) lv_obj_remove_flag(object,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(object,LV_OBJ_FLAG_HIDDEN);
}

lv_point_precise_t polar(int cx,int cy,double degrees,int radius)
{
    const double angle=(degrees-90.0)*PI/180.0;
    return {cx+static_cast<int>(std::lround(radius*std::cos(angle))),
            cy+static_cast<int>(std::lround(radius*std::sin(angle)))};
}

lv_draw_line_dsc_t hand_line(int cx,int cy,double angle,int length,int tail,int width,uint32_t color)
{
    const double radians=(angle-90.0)*PI/180.0;
    const double dx=std::cos(radians),dy=std::sin(radians);
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.p1={cx+static_cast<int>(std::lround(-tail*dx)),cy+static_cast<int>(std::lround(-tail*dy))};
    line.p2={cx+static_cast<int>(std::lround(length*dx)),cy+static_cast<int>(std::lround(length*dy))};
    line.width=width; line.color=lv_color_hex(color);
    line.round_start=1; line.round_end=1;
    return line;
}

void shadow(lv_layer_t *layer,lv_draw_line_dsc_t line,int width)
{
    // Translate a copy so both shadows can stay below the unchanged hands.
    ++line.p1.x; line.p1.y+=2;
    ++line.p2.x; line.p2.y+=2;
    line.width=width; line.color=lv_color_hex(BEZEL);
    lv_draw_line(layer,&line);
}

void circle(lv_layer_t *layer,int cx,int cy,int radius,uint32_t color,int stroke=0)
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color=lv_color_hex(color); dsc.radius=LV_RADIUS_CIRCLE;
    if (stroke) {
        dsc.bg_opa=LV_OPA_TRANSP;
        dsc.border_width=stroke; dsc.border_color=lv_color_hex(color);
    }
    const lv_area_t area={cx-radius,cy-radius,cx+radius,cy+radius};
    lv_draw_rect(layer,&dsc,&area);
}
}

namespace esp_brookesia::apps {
Watch *Watch::instance_;
Watch *Watch::requestInstance()
{
    if (!instance_) instance_=new(std::nothrow) Watch;
    return instance_;
}
Watch::Watch():
    App(systems::base::App::Config::SIMPLE_CONSTRUCTOR("Watch",&img_app_watch,true),fullscreen_config()) {}
Watch::~Watch() { close(); instance_=nullptr; }

bool Watch::run()
{
    mode_=0; valid_=false; touch_active_=swiped_=closing_=false;
    last_second_=(time_t)-1; last_source_=WIFI_CLOCK_UNSET;
    root_=app_ui::box(lv_screen_active(),0,0,SIZE,SIZE,FACE,0);
    lv_obj_add_flag(root_,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_remove_flag(root_,LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(root_,touch,LV_EVENT_ALL,this);
    dial_=app_ui::box(root_,0,0,SIZE,SIZE,FACE,0);
    lv_obj_add_event_cb(dial_,draw_dial,LV_EVENT_DRAW_MAIN,this);
    time_=centered(root_,"",78,&lv_font_montserrat_32,INK);
    date_=centered(root_,"",240,&lv_font_montserrat_14,MUTED);
    source_=centered(root_,"",266,&lv_font_montserrat_12,MUTED);
    waiting_=centered(root_,"Waiting for time",152,&lv_font_montserrat_20,INK);
    help_=centered(root_,"Connect Wi-Fi to sync",190,&lv_font_montserrat_14,MUTED);
    // Hands are drawn above the optional calendar, with no image decoder,
    // framebuffer allocation, filesystem access, or dedicated app worker.
    hands_=app_ui::box(root_,0,0,SIZE,SIZE,FACE,0);
    lv_obj_set_style_bg_opa(hands_,LV_OPA_TRANSP,0);
    lv_obj_add_event_cb(hands_,draw_hands,LV_EVENT_DRAW_MAIN,this);
    button_presses_=bsp_user_button_get_press_count();
    refresh();
    apply_mode();
    timer_=lv_timer_create(tick,33,this);
    if (!timer_) { close(); return false; }
    return true;
}

void Watch::draw_dial(lv_event_t *event)
{
    auto *self=static_cast<Watch *>(lv_event_get_user_data(event));
    if (self->mode_==2) return;
    auto *layer=lv_event_get_layer(event);
    lv_area_t bounds; lv_obj_get_coords(self->dial_,&bounds);
    const int cx=bounds.x1+CENTER,cy=bounds.y1+CENTER;
    // The dial already supplies the ivory fill; draw only the bezel outlines.
    circle(layer,cx,cy,179,RIM,2);
    circle(layer,cx,cy,177,BEZEL,3);
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    for (int index=0;index<12;++index) {
        const bool major=index%3==0;
        line.p1=polar(cx,cy,index*30.0,major?132:143);
        line.p2=polar(cx,cy,index*30.0,160);
        line.width=major?5:3;
        line.color=lv_color_hex(major?0x333b45:0x92948f);
        line.round_start=1; line.round_end=1;
        lv_draw_line(layer,&line);
    }
}

void Watch::draw_hands(lv_event_t *event)
{
    auto *self=static_cast<Watch *>(lv_event_get_user_data(event));
    if (!self->valid_) return;
    auto *layer=lv_event_get_layer(event);
    lv_area_t bounds; lv_obj_get_coords(self->hands_,&bounds);
    const int cx=bounds.x1+CENTER,cy=bounds.y1+CENTER;
    const auto hour=hand_line(cx,cy,self->angles_.hour,88,10,10,INK);
    const auto minute=hand_line(cx,cy,self->angles_.minute,121,12,7,INK);
    const auto second=hand_line(cx,cy,self->angles_.second,148,23,2,RED);
    shadow(layer,hour,12);
    shadow(layer,minute,9);
    lv_draw_line(layer,&hour);
    lv_draw_line(layer,&minute);
    circle(layer,cx,cy,7,INK);
    lv_draw_line(layer,&second);
    circle(layer,cx,cy,5,RED);
    circle(layer,cx,cy,2,FACE);
}

void Watch::refresh()
{
    wifi_clock_snapshot_t clock;
    const bool valid=wifi_manager_get_clock(&clock);
    const bool changed=valid_!=valid;
    valid_=valid;
    if (valid) {
        const struct tm &local=clock.local;
        angles_=watch_angles(local.tm_hour,local.tm_min,local.tm_sec,clock.instant.tv_usec);
        if (last_second_!=clock.instant.tv_sec || last_source_!=clock.source || changed) {
            char text[48];
            std::snprintf(text,sizeof(text),"%02d:%02d",local.tm_hour,local.tm_min);
            app_ui::text(time_,text);
            static const char *DAYS[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
            std::snprintf(text,sizeof(text),"%s  /  %04d.%02d.%02d",DAYS[local.tm_wday],
                          local.tm_year+1900,local.tm_mon+1,local.tm_mday);
            app_ui::text(date_,text);
            app_ui::text(source_,clock.source==WIFI_CLOCK_NETWORK?"SYNCED  /  UTC+8":"RTC  /  UTC+8");
            last_second_=clock.instant.tv_sec; last_source_=clock.source;
        }
    }
    if (changed) apply_mode();
    // Waiting for the first valid clock does not require continuous repainting.
    if (valid || changed) lv_obj_invalidate(hands_);
}

void Watch::apply_mode()
{
    const bool info=valid_ && mode_==1;
    visible(time_,info); visible(date_,info); visible(source_,info);
    visible(waiting_,!valid_); visible(help_,!valid_);
    lv_obj_invalidate(dial_);
}

void Watch::touch(lv_event_t *event)
{
    auto *self=static_cast<Watch *>(lv_event_get_user_data(event));
    auto *input=lv_indev_active();
    if (!input || self->closing_) return;
    const auto code=lv_event_get_code(event);
    if (code==LV_EVENT_PRESSED) {
        lv_indev_get_point(input,&self->touch_start_);
        self->touch_active_=true; self->swiped_=false;
    } else if (code==LV_EVENT_PRESS_LOST) {
        self->touch_active_=false; self->swiped_=true;
    } else if (code==LV_EVENT_PRESSING && self->touch_active_) {
        lv_point_t point; lv_indev_get_point(input,&point);
        if (std::abs(point.x-self->touch_start_.x)>18 || std::abs(point.y-self->touch_start_.y)>18)
            self->swiped_=true;
    } else if (code==LV_EVENT_RELEASED && self->touch_active_) {
        self->touch_active_=false;
        lv_point_t point; lv_indev_get_point(input,&point);
        const int dx=point.x-self->touch_start_.x,dy=point.y-self->touch_start_.y;
        if (std::abs(dx)>18 || std::abs(dy)>18) self->swiped_=true;
        if (self->touch_start_.y>=290 && dy<=-50 && -dy>std::abs(dx)) self->back();
    } else if (code==LV_EVENT_SHORT_CLICKED && !self->swiped_) {
        self->mode_=(self->mode_+1)%3;
        self->apply_mode();
    }
}

void Watch::tick(lv_timer_t *timer)
{
    auto *self=static_cast<Watch *>(lv_timer_get_user_data(timer));
    if (self->closing_) return;
    const uint32_t presses=bsp_user_button_get_press_count();
    if (presses!=self->button_presses_) {
        self->button_presses_=presses;
        self->back();
        return;
    }
    self->refresh();
}

bool Watch::back()
{
    if (closing_) return true;
    closing_=notifyCoreClosed();
    return closing_;
}
bool Watch::pause()
{
    touch_active_=false; swiped_=true;
    if (timer_) lv_timer_pause(timer_);
    return true;
}
bool Watch::resume()
{
    if (!root_) return false;
    closing_=false; touch_active_=false; swiped_=true;
    button_presses_=bsp_user_button_get_press_count();
    refresh();
    if (timer_) lv_timer_resume(timer_);
    return true;
}
bool Watch::close()
{
    if (timer_) { lv_timer_delete(timer_); timer_=nullptr; }
    if (root_) lv_obj_delete(root_);
    root_=dial_=hands_=time_=date_=source_=waiting_=help_=nullptr;
    valid_=touch_active_=false; closing_=false;
    return true;
}
}
