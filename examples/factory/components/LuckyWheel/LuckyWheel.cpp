/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 *
 * Lucky-wheel behavior adapted for the Brookesia desktop. The round pastel
 * interface and launcher artwork are specific to this product.
 */
#include "LuckyWheel.hpp"
#include "app_support.hpp"
#include "esp_random.h"
#include "esp_timer.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>

LV_IMAGE_DECLARE(img_app_lucky_wheel);

namespace {
// Bleed two pixels past the round panel's radius to avoid a pale edge from
// antialiasing. LVGL clips the drawing to the existing 360 x 360 surface.
constexpr int SIZE=360,CENTER=180,RADIUS=CENTER+2;
constexpr uint32_t CREAM=0xfbf5e9,INK=0x2e342f,GREEN=0x334d43,MUTED=0x68766b;
constexpr uint32_t COLORS[]={0xf6a79a,0xf1ca86,0xc9dda0,0x96d3bc,
                             0x9ccce7,0xbab6e5,0xe4bad2,0xedbc9f};
constexpr const char *NUMBERS[]={"01","02","03","04","05","06","07","08","09",
                                  "10","11","12","13","14","15","16","17","18"};
constexpr double PI=3.14159265358979323846;

esp_brookesia::systems::phone::App::Config fullscreen_config()
{
    auto cfg=esp_brookesia::systems::phone::App::Config::SIMPLE_CONSTRUCTOR(&img_app_lucky_wheel,false,false);
    cfg.flags.enable_navigation_gesture=false;
    return cfg;
}

uint32_t random_below(uint32_t bound)
{
    // Reject the incomplete remainder so every choice has the same chance.
    uint32_t threshold=(uint32_t)(-bound)%bound,value;
    do { value=esp_random(); } while (value<threshold);
    return value%bound;
}

lv_obj_t *centered(lv_obj_t *parent,const char *text,int y,int width,
                   const lv_font_t *font,uint32_t color)
{
    auto *label=app_ui::label(parent,text,0,y,width,font,color);
    lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_align(label,LV_ALIGN_TOP_MID,0,y);
    return label;
}

void triangle(lv_layer_t *layer,int x,int y,int half,int height,uint32_t color)
{
    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.color=lv_color_hex(color);
    dsc.p[0]={x-half,y}; dsc.p[1]={x+half,y}; dsc.p[2]={x,y+height};
    lv_draw_triangle(layer,&dsc);
}
}

namespace esp_brookesia::apps {
LuckyWheel *LuckyWheel::instance_;
LuckyWheel *LuckyWheel::requestInstance()
{
    if (!instance_) instance_=new(std::nothrow) LuckyWheel;
    return instance_;
}
LuckyWheel::LuckyWheel():
    App(systems::base::App::Config::SIMPLE_CONSTRUCTOR("Lucky Wheel",&img_app_lucky_wheel,true),fullscreen_config()) {}
LuckyWheel::~LuckyWheel() { close(); instance_=nullptr; }

bool LuckyWheel::run()
{
    rotation_=0; spinning_=false; touch_active_=false; paused_at_=0;
    root_=app_ui::box(lv_screen_active(),0,0,SIZE,SIZE,CREAM,0);
    lv_obj_add_flag(root_,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_remove_flag(root_,LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(root_,touch,LV_EVENT_ALL,this);
    wheel_=app_ui::box(root_,0,0,SIZE,SIZE,CREAM,0);
    lv_obj_set_style_bg_opa(wheel_,LV_OPA_TRANSP,0);
    lv_obj_add_event_cb(wheel_,draw,LV_EVENT_DRAW_MAIN,this);

    // All geometry uses the native 360 x 360 screen. No rotated full-screen
    // bitmap, decoder task, Flash write, or additional framebuffer is needed.
    auto *hub_rim=app_ui::box(root_,119,119,122,122,CREAM,61);
    (void)hub_rim;
    hub_=app_ui::box(root_,124,124,112,112,GREEN,56);
    lv_obj_add_flag(hub_,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_user_data(hub_,reinterpret_cast<void *>(1));
    lv_obj_add_event_cb(hub_,action,LV_EVENT_SHORT_CLICKED,this);
    lv_obj_set_style_bg_color(hub_,lv_color_hex(0x294036),LV_STATE_PRESSED);
    caption_=centered(hub_,"READY?",18,112,&lv_font_montserrat_12,0xd7e1d6);
    result_=centered(hub_,"SPIN",38,112,&lv_font_montserrat_24,CREAM);
    hint_=centered(hub_,"Tap to start",72,112,&lv_font_montserrat_12,0xd7e1d6);

    options_=app_ui::button(root_,LV_SYMBOL_SETTINGS "  Options",116,306,128,44,2,action,this);
    lv_obj_set_style_radius(options_,22,0);
    lv_obj_set_style_bg_color(options_,lv_color_hex(CREAM),0);
    lv_obj_set_style_bg_color(options_,lv_color_hex(0xece5d6),LV_STATE_PRESSED);
    button_presses_=bsp_user_button_get_press_count();
    timer_=lv_timer_create(tick,33,this);
    if (!timer_) { close(); return false; }
    return true;
}

void LuckyWheel::draw(lv_event_t *event)
{
    auto *self=static_cast<LuckyWheel *>(lv_event_get_user_data(event));
    auto *layer=lv_event_get_layer(event);
    lv_area_t bounds;
    lv_obj_get_coords(self->wheel_,&bounds);
    const int cx=bounds.x1+CENTER,cy=bounds.y1+CENTER;
    const double span=360.0/self->count_;
    const double rotation=self->rotation_/10.0;
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.center={cx,cy}; arc.radius=RADIUS; arc.width=RADIUS; arc.rounded=0;
    for (int i=0;i<self->count_;++i) {
        const double start=-90.0-span/2+i*span+rotation;
        // The software arc renderer requires nonnegative angles, including
        // during counterclockwise spins and across the zero-degree boundary.
        arc.start_angle=(static_cast<int>(std::lround(start))%360+360)%360;
        arc.end_angle=(static_cast<int>(std::lround(start+span))%360+360)%360;
        int color=self->count_<=8?i*8/self->count_:i%8;
        if (i==self->count_-1 && color==0) color=2;
        arc.color=lv_color_hex(COLORS[color]);
        lv_draw_arc(layer,&arc);
    }
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.width=3; line.color=lv_color_hex(CREAM);
    for (int i=0;i<self->count_;++i) {
        const double a=(-90.0-span/2+i*span+rotation)*PI/180.0;
        line.p1={cx,cy};
        line.p2={cx+static_cast<int>(std::lround(RADIUS*std::cos(a))),
                 cy+static_cast<int>(std::lround(RADIUS*std::sin(a)))};
        lv_draw_line(layer,&line);
    }
    const lv_font_t *font=self->count_<=8?&lv_font_montserrat_24:
                          self->count_<=12?&lv_font_montserrat_20:&lv_font_montserrat_18;
    const int radius=self->count_<=8?96:104;
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.font=font; label.color=lv_color_hex(INK); label.align=LV_TEXT_ALIGN_CENTER;
    label.text_static=1;
    for (int i=0;i<self->count_;++i) {
        const double a=(-90.0+i*span+rotation)*PI/180.0;
        const int x=cx+static_cast<int>(std::lround(radius*std::cos(a)));
        const int y=cy+static_cast<int>(std::lround(radius*std::sin(a)));
        lv_area_t area={x-21,y-font->line_height/2,x+20,y+font->line_height/2};
        label.text=NUMBERS[i];
        lv_draw_label(layer,&label,&area);
    }
    // The pointer stays fixed. Numbers remain upright while orbiting so the
    // result remains legible at any wheel angle on the small round display.
    triangle(layer,cx,bounds.y1+43,12,22,GREEN);
    triangle(layer,cx,bounds.y1+47,7,12,CREAM);
}

void LuckyWheel::start_spin()
{
    if (spinning_ || menu_) return;
    const int slot=random_below(count_);
    const int half=1800/count_;
    const int margin=(3600/count_)*22/100<80?(3600/count_)*22/100:80;
    const int range=half-margin;
    const int offset=static_cast<int>(random_below(range*2+1))-range;
    spin_start_=wheel_normalize(rotation_);
    spin_target_=wheel_spin_target(spin_start_,count_,slot,4+random_below(4),random_below(2),offset);
    duration_=2400+random_below(1201);
    started_at_=esp_timer_get_time(); spinning_=true;
    app_ui::enabled(hub_,false); app_ui::enabled(options_,false);
    // Keep the hub fully opaque while disabling input; its contrast is fixed.
    app_ui::text(caption_,"GOOD LUCK"); app_ui::text(result_,"..."); app_ui::text(hint_,"Spinning");
}

void LuckyWheel::reset_result()
{
    spinning_=false; rotation_=0;
    app_ui::enabled(hub_,true); app_ui::enabled(options_,true);
    app_ui::text(caption_,"READY?"); app_ui::text(result_,"SPIN"); app_ui::text(hint_,"Tap to start");
    lv_obj_invalidate(wheel_);
}

void LuckyWheel::show_options()
{
    if (menu_ || spinning_) return;
    pending_count_=count_;
    touch_active_=false;
    menu_=app_ui::box(root_,0,0,SIZE,SIZE,CREAM,0);
    lv_obj_add_flag(menu_,LV_OBJ_FLAG_CLICKABLE);
    centered(menu_,"Wheel options",46,264,&lv_font_montserrat_22,INK);
    centered(menu_,"Number of choices",90,252,&lv_font_montserrat_14,MUTED);
    minus_=app_ui::button(menu_,LV_SYMBOL_MINUS,68,128,52,52,3,action,this);
    plus_=app_ui::button(menu_,LV_SYMBOL_PLUS,240,128,52,52,4,action,this);
    for (auto *button:{minus_,plus_}) {
        lv_obj_set_style_radius(button,26,0);
        lv_obj_set_style_bg_color(button,lv_color_hex(0xece5d6),0);
    }
    count_label_=centered(menu_,"",137,104,&lv_font_montserrat_32,GREEN);
    centered(menu_,"2 to 18 choices",194,252,&lv_font_montserrat_12,MUTED);
    auto *done=app_ui::button(menu_,"Done",84,228,192,44,5,action,this,true);
    lv_obj_set_style_radius(done,22,0);
    lv_obj_set_style_bg_color(done,lv_color_hex(GREEN),0);
    lv_obj_set_style_bg_color(done,lv_color_hex(0x294036),LV_STATE_PRESSED);
    auto *exit=app_ui::button(menu_,"Exit to desktop",100,284,160,44,6,action,this);
    lv_obj_set_style_radius(exit,22,0);
    lv_obj_set_style_bg_color(exit,lv_color_hex(0xece5d6),0);
    refresh_count();
}

void LuckyWheel::refresh_count()
{
    char text[8]; snprintf(text,sizeof(text),"%d",pending_count_);
    app_ui::text(count_label_,text);
    app_ui::enabled(minus_,pending_count_>WHEEL_MIN_CHOICES);
    app_ui::enabled(plus_,pending_count_<WHEEL_MAX_CHOICES);
}

void LuckyWheel::hide_options(bool apply)
{
    if (!menu_) return;
    if (apply && pending_count_!=count_) { count_=pending_count_; reset_result(); }
    lv_obj_delete(menu_);
    menu_=count_label_=minus_=plus_=nullptr;
}

void LuckyWheel::action(lv_event_t *event)
{
    auto *self=static_cast<LuckyWheel *>(lv_event_get_user_data(event));
    switch (app_ui::action(event)) {
    case 1: self->start_spin(); break;
    case 2: self->show_options(); break;
    case 3:
        if (self->pending_count_>WHEEL_MIN_CHOICES) --self->pending_count_;
        self->refresh_count(); break;
    case 4:
        if (self->pending_count_<WHEEL_MAX_CHOICES) ++self->pending_count_;
        self->refresh_count(); break;
    case 5: self->hide_options(true); break;
    case 6: self->notifyCoreClosed(); break;
    default: break;
    }
}

void LuckyWheel::touch(lv_event_t *event)
{
    auto *self=static_cast<LuckyWheel *>(lv_event_get_user_data(event));
    auto *input=lv_indev_active();
    if (!input || self->menu_) return;
    const auto code=lv_event_get_code(event);
    if (code==LV_EVENT_PRESS_LOST) { self->touch_active_=false; return; }
    if (code==LV_EVENT_PRESSED) {
        lv_indev_get_point(input,&self->touch_start_); self->touch_active_=true;
    }
    if (code!=LV_EVENT_RELEASED || !self->touch_active_) return;
    self->touch_active_=false;
    lv_point_t point; lv_indev_get_point(input,&point);
    int dx=point.x-self->touch_start_.x,dy=point.y-self->touch_start_.y;
    if (self->touch_start_.y>=290 && dy<=-50 && -dy>std::abs(dx)) self->back();
}

void LuckyWheel::tick(lv_timer_t *timer)
{
    auto *self=static_cast<LuckyWheel *>(lv_timer_get_user_data(timer));
    const uint32_t presses=bsp_user_button_get_press_count();
    if (self->button_presses_!=presses) { self->button_presses_=presses; self->notifyCoreClosed(); return; }
    if (!self->spinning_) return;
    const uint32_t elapsed=static_cast<uint32_t>((esp_timer_get_time()-self->started_at_)/1000);
    self->rotation_=wheel_ease_out(self->spin_start_,self->spin_target_,elapsed,self->duration_);
    lv_obj_invalidate(self->wheel_);
    if (elapsed<self->duration_) return;
    self->rotation_=wheel_normalize(self->spin_target_);
    self->spinning_=false;
    char text[8]; snprintf(text,sizeof(text),"%02d",wheel_result(self->rotation_,self->count_)+1);
    app_ui::text(self->caption_,"YOUR PICK"); app_ui::text(self->result_,text); app_ui::text(self->hint_,"Spin again");
    app_ui::enabled(self->hub_,true); app_ui::enabled(self->options_,true);
}

bool LuckyWheel::back()
{
    if (menu_) { hide_options(false); return true; }
    return notifyCoreClosed();
}
bool LuckyWheel::pause()
{
    paused_at_=esp_timer_get_time(); touch_active_=false;
    if (timer_) lv_timer_pause(timer_);
    return true;
}
bool LuckyWheel::resume()
{
    if (paused_at_ && spinning_) started_at_+=esp_timer_get_time()-paused_at_;
    paused_at_=0; button_presses_=bsp_user_button_get_press_count();
    if (timer_) lv_timer_resume(timer_);
    return true;
}
bool LuckyWheel::close()
{
    if (timer_) { lv_timer_delete(timer_); timer_=nullptr; }
    spinning_=false; touch_active_=false; paused_at_=0;
    if (root_) lv_obj_delete(root_);
    root_=wheel_=hub_=options_=caption_=result_=hint_=nullptr;
    menu_=count_label_=minus_=plus_=nullptr;
    return true;
}
}
