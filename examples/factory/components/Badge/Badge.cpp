#include "Badge.hpp"
#include "wifi_manager.h"
#include "esp_timer.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

LV_IMAGE_DECLARE(img_app_badge);
namespace {
constexpr uint32_t BG=0xf5f3ed,INK=0x283b40,GREEN=0x496c61,MUTED=0x81908e;
constexpr uint32_t SOFT=0xe8ece6,RING=0xe2e6df,PRESSED=0xd7e0d6;

lv_obj_t *badge_button(lv_obj_t *parent,const char *text,int x,int y,int w,int h,
                       int id,lv_event_cb_t callback,void *context,bool primary=false,
                       const lv_font_t *font=&lv_font_montserrat_16)
{
    auto *obj=app_ui::button(parent,text,x,y,w,h,id,callback,context);
    lv_obj_set_style_radius(obj,h/2,0);
    lv_obj_set_style_bg_color(obj,lv_color_hex(primary?GREEN:SOFT),0);
    lv_obj_set_style_bg_color(obj,lv_color_hex(primary?0x375448:PRESSED),LV_STATE_PRESSED);
    auto *label=lv_obj_get_child(obj,0);
    lv_obj_set_style_text_color(label,lv_color_hex(primary?0xffffff:GREEN),0);
    lv_obj_set_style_text_font(label,font,0);
    return obj;
}
}
namespace esp_brookesia::apps {
Badge *Badge::instance_;
Badge *Badge::requestInstance() { if (!instance_) instance_=new(std::nothrow) Badge; return instance_; }
Badge::Badge():App("Badge",&img_app_badge,true,false,false) {}
Badge::~Badge() { close(); if (frame_mutex_) vSemaphoreDelete(frame_mutex_); instance_=nullptr; }

bool Badge::run()
{
    if (!badge::prepare()) return false;
    if (!frame_mutex_) frame_mutex_=xSemaphoreCreateMutex();
    if (!frame_mutex_) return false;
    display_=static_cast<uint16_t *>(heap_caps_aligned_calloc(16,1,badge::PIXEL_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    pending_=static_cast<uint16_t *>(heap_caps_aligned_alloc(16,badge::PIXEL_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if (!display_ || !pending_) { close(); return false; }
    frame_ready_=false; direction_=0; state_={}; touch_active_=false;
    frame_revision_=display_revision_=0; display_valid_=false;
    root_=app_ui::box(lv_screen_active(),0,0,360,360,BG,0);
    lv_obj_add_flag(root_,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(root_,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(root_,event,LV_EVENT_ALL,this);
    image_={}; image_.header.magic=LV_IMAGE_HEADER_MAGIC;
    image_.header.cf=LV_COLOR_FORMAT_RGB565;
    image_.header.w=360; image_.header.h=360; image_.header.stride=720;
    image_.data_size=badge::PIXEL_BYTES; image_.data=reinterpret_cast<uint8_t *>(display_);
    image_obj_=lv_image_create(root_); lv_image_set_src(image_obj_,&image_);
    lv_obj_remove_flag(image_obj_,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(image_obj_,0,0);
    lv_obj_add_flag(image_obj_,LV_OBJ_FLAG_HIDDEN);
    create_welcome();
    message_=app_ui::label(root_,"Preparing badge...",55,132,250,&lv_font_montserrat_18,INK);
    lv_obj_set_style_text_align(message_,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_add_flag(message_,LV_OBJ_FLAG_HIDDEN);
    timer_=lv_timer_create(poll,100,this);
    if (!timer_ || !start("badge",6144,2,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)) { close(); return false; }
    return true;
}
void Badge::create_welcome()
{
    // Keep the content inside the round screen's visible area. Static LVGL
    // shapes reuse built-in fonts without allocating another image buffer.
    welcome_=app_ui::box(root_,0,0,360,360,BG,0);
    auto centered=[&](const char *text,int y,int width,const lv_font_t *font,uint32_t color) {
        auto *label=app_ui::label(welcome_,text,(360-width)/2,y,width,font,color);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
        return label;
    };
    centered("PHOTO BADGE",38,180,&lv_font_montserrat_12,GREEN);
    auto *emblem=app_ui::box(welcome_,144,66,72,72,SOFT,36);
    lv_obj_set_style_border_width(emblem,1,0);
    lv_obj_set_style_border_color(emblem,lv_color_hex(RING),0);
    auto *symbol=app_ui::label(emblem,LV_SYMBOL_IMAGE,0,0,72,&lv_font_montserrat_32,GREEN);
    lv_obj_set_style_text_align(symbol,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_center(symbol);
    auto *plus=app_ui::box(welcome_,198,112,24,24,GREEN,12);
    auto *plus_text=app_ui::label(plus,LV_SYMBOL_PLUS,0,0,24,&lv_font_montserrat_12,0xffffff);
    lv_obj_set_style_text_align(plus_text,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_center(plus_text);
    centered("Make it yours",150,264,&lv_font_montserrat_24,INK);
    welcome_text_=centered("Preparing your badge...",188,268,&lv_font_montserrat_14,MUTED);
    lv_obj_set_style_text_line_space(welcome_text_,4,0);
    welcome_button_=badge_button(welcome_,LV_SYMBOL_PLUS "  Add images",86,242,188,46,4,action,this,true);
    app_ui::enabled(welcome_button_,false);
    centered("Tap for controls",299,220,&lv_font_montserrat_12,GREEN);
    centered("Swipe up to exit",320,160,&lv_font_montserrat_12,MUTED);
}
void Badge::work()
{
    bool initialized=badge::initialize();
    uint32_t loaded=0;
    while (!stopping()) {
        int direction=direction_.exchange(0);
        if (direction && initialized) badge::next(direction);
        badge::State s;
        if (badge::snapshot(s) && (s.revision!=loaded || !initialized)) {
            // The UI only tries this lock; JPEG decoding never stalls touch input.
            xSemaphoreTake(frame_mutex_,portMAX_DELAY);
            frame_ready_=false; frame_valid_=false; frame_empty_=initialized && !s.available; frame_message_[0]=0;
            frame_revision_=s.revision;
            if (!initialized) snprintf(frame_message_,sizeof(frame_message_),"%s",s.error);
            else if (s.available) frame_valid_=badge::load(s.active,pending_,frame_message_,sizeof(frame_message_));
            frame_ready_=true;
            xSemaphoreGive(frame_mutex_);
            loaded=s.revision;
            if (!initialized) break;
        }
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}
void Badge::refresh()
{
    const uint32_t previous_revision=state_.revision;
    if (badge::snapshot(state_) && state_.revision!=previous_revision) controls_dirty_=true;
    if (xSemaphoreTake(frame_mutex_,0)==pdTRUE) {
        if (frame_ready_) {
            controls_dirty_=true;
            display_revision_=frame_revision_;
            display_valid_=frame_valid_;
            if (frame_valid_) {
                memcpy(display_,pending_,badge::PIXEL_BYTES);
                lv_image_cache_drop(&image_); lv_obj_invalidate(image_obj_);
                if (thumbnail_) lv_obj_invalidate(thumbnail_);
                lv_obj_remove_flag(image_obj_,LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(message_,LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(welcome_,LV_OBJ_FLAG_HIDDEN);
            } else if (frame_empty_) {
                lv_obj_add_flag(image_obj_,LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(message_,LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(welcome_,LV_OBJ_FLAG_HIDDEN);
                app_ui::text(welcome_text_,"Upload photos from your phone.\nSave up to 6 favorites.");
                app_ui::enabled(welcome_button_,true);
            } else {
                lv_obj_add_flag(image_obj_,LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(welcome_,LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(message_,LV_OBJ_FLAG_HIDDEN);
                app_ui::text(message_,frame_message_);
            }
            frame_ready_=false;
        }
        xSemaphoreGive(frame_mutex_);
    }
    update_controls();
    if (!editing_) return;
    if (badge::editor_finished()) { end_editor(); return; }
    wifi_manager_snapshot_t wifi={}; wifi_manager_get_snapshot(&wifi);
    if (wifi.provisioning) portal_seen_=true;
    char text[240];
    if (wifi.provisioning) snprintf(text,sizeof(text),"Connect phone to\n%s\nPassword: %s\n\nOpen %s",
                                  wifi.ap_ssid,WIFI_PROVISION_PASSWORD,WIFI_PROVISION_URL);
    else if (portal_seen_ || esp_timer_get_time()-editor_started_>15000000)
        snprintf(text,sizeof(text),"Hotspot unavailable.\nTap Done, then try again.");
    else snprintf(text,sizeof(text),"Starting hotspot...");
    app_ui::text(connection_,text);
}
void Badge::show_dialog()
{
    if (editing_ || dialog_ || !state_.ready) return;
    controls_dirty_=true;
    dialog_=app_ui::box(root_,0,0,360,360,BG,0);
    lv_obj_add_flag(dialog_,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(dialog_,LV_OBJ_FLAG_GESTURE_BUBBLE);
    auto centered=[&](const char *text,int y,const lv_font_t *font,uint32_t color) {
        auto *label=app_ui::label(dialog_,text,50,y,260,font,color);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
        return label;
    };
    centered("Photo badge",40,&lv_font_montserrat_24,INK);
    counter_=centered("",75,&lv_font_montserrat_14,MUTED);
    auto *ring=app_ui::box(dialog_,117,93,126,126,RING,LV_RADIUS_CIRCLE);
    auto *crop=app_ui::box(ring,5,5,116,116,RING,LV_RADIUS_CIRCLE);
    lv_obj_set_style_clip_corner(crop,true,0);
    // Share the decoded full-screen frame. LVGL scales and clips the preview;
    // opening controls never reads Flash or allocates another photo buffer.
    thumbnail_=lv_image_create(crop);
    lv_image_set_src(thumbnail_,&image_);
    lv_obj_set_size(thumbnail_,116,116);
    lv_obj_set_pos(thumbnail_,0,0);
    lv_image_set_inner_align(thumbnail_,LV_IMAGE_ALIGN_STRETCH);
    lv_image_set_antialias(thumbnail_,true);
    lv_obj_remove_flag(thumbnail_,LV_OBJ_FLAG_CLICKABLE);
    preview_hint_=app_ui::label(crop,"",0,0,110,&lv_font_montserrat_12,GREEN);
    lv_obj_set_style_text_align(preview_hint_,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_center(preview_hint_);
    previous_=badge_button(dialog_,LV_SYMBOL_LEFT,41,133,46,46,6,action,this,false,&lv_font_montserrat_18);
    next_=badge_button(dialog_,LV_SYMBOL_RIGHT,273,133,46,46,7,action,this,false,&lv_font_montserrat_18);
    for (auto &dot:dots_) dot=app_ui::box(dialog_,0,228,6,6,0xd0d8cf,LV_RADIUS_CIRCLE);
    badge_button(dialog_,LV_SYMBOL_IMAGE "  Manage images",70,251,220,46,2,action,this,true);
    auto *exit=badge_button(dialog_,"Exit",85,303,76,40,5,action,this,false,&lv_font_montserrat_14);
    auto *close=badge_button(dialog_,"Close",199,303,76,40,1,action,this,false,&lv_font_montserrat_14);
    lv_obj_set_style_bg_color(exit,lv_color_hex(BG),0);
    lv_obj_set_style_bg_color(close,lv_color_hex(BG),0);
    lv_obj_set_style_text_color(lv_obj_get_child(exit,0),lv_color_hex(0x8b7772),0);
    app_ui::box(dialog_,180,316,1,17,0xd5dbd2,0);
    update_controls();
}
void Badge::close_dialog()
{
    if (dialog_) lv_obj_delete(dialog_);
    dialog_=nullptr;
    clear_controls();
}
void Badge::clear_controls()
{
    counter_=previous_=next_=nullptr;
    thumbnail_=preview_hint_=nullptr;
    for (auto &dot:dots_) dot=nullptr;
    controls_dirty_=true;
}
void Badge::update_controls()
{
    // Selection, storage and decoder completion are the only menu inputs.
    if (!counter_ || !controls_dirty_) return;
    controls_dirty_=false;
    int count=0,index=0;
    for (int slot=0;slot<badge::SLOTS;++slot) {
        if (!(state_.available&(1<<slot))) continue;
        ++count;
        if (slot==state_.active) index=count;
    }
    char text[48];
    if (!count) snprintf(text,sizeof(text),"No saved images");
    else snprintf(text,sizeof(text),"Image %d of %d",index,count);
    app_ui::text(counter_,text);
    app_ui::enabled(previous_,count>1);
    app_ui::enabled(next_,count>1);
    for (int i=0;i<badge::SLOTS;++i) {
        auto *dot=dots_[i];
        if (i>=count) { lv_obj_add_flag(dot,LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(dot,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(dot,180-(count-1)*6+i*12-3,228);
        lv_obj_set_style_bg_color(dot,lv_color_hex(i==index-1?GREEN:0xd0d8cf),0);
    }
    // A selection/upload can precede its worker decode. Hide the previous
    // frame until its revision matches the selected image shown by the dots.
    bool current=count && display_valid_ && display_revision_==state_.revision;
    if (current) {
        lv_obj_remove_flag(thumbnail_,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(preview_hint_,LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(thumbnail_,LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(preview_hint_,LV_OBJ_FLAG_HIDDEN);
        app_ui::text(preview_hint_,!count?"No image":
                     display_revision_==state_.revision?"Unavailable":"Loading...");
        lv_obj_center(preview_hint_);
    }
}
void Badge::open_editor()
{
    close_dialog();
    if (!badge::start_editor()) {
        dialog_=app_ui::box(root_,0,0,360,360,BG,0);
        auto *title=app_ui::label(dialog_,"Wi-Fi is busy",50,64,260,&lv_font_montserrat_22,INK);
        lv_obj_set_style_text_align(title,LV_TEXT_ALIGN_CENTER,0);
        auto *l=app_ui::label(dialog_,"Close Wi-Fi setup\nand try again.",50,145,260,&lv_font_montserrat_16,MUTED);
        lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
        badge_button(dialog_,"OK",86,244,188,44,1,action,this,true);
        return;
    }
    editing_=true; portal_seen_=false; editor_started_=esp_timer_get_time();
    editor_=app_ui::box(root_,0,0,360,360,BG,0);
    auto *title=app_ui::label(editor_,"Manage images",50,40,260,&lv_font_montserrat_22,INK);
    lv_obj_set_style_text_align(title,LV_TEXT_ALIGN_CENTER,0);
    auto *subtitle=app_ui::label(editor_,"Connect your phone",50,69,260,&lv_font_montserrat_12,MUTED);
    lv_obj_set_style_text_align(subtitle,LV_TEXT_ALIGN_CENTER,0);
    auto *card=app_ui::box(editor_,52,96,256,150,SOFT,18);
    connection_=app_ui::label(card,"Starting hotspot...",12,14,232,&lv_font_montserrat_14,GREEN);
    lv_obj_set_style_text_align(connection_,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_line_space(connection_,4,0);
    badge_button(editor_,"Done",96,264,168,44,3,action,this,true);
    auto *hint=app_ui::label(editor_,"Then choose Transfer Images",50,315,260,&lv_font_montserrat_12,MUTED);
    lv_obj_set_style_text_align(hint,LV_TEXT_ALIGN_CENTER,0);
}
void Badge::end_editor()
{
    if (editing_) badge::stop_editor();
    editing_=false;
    if (editor_) lv_obj_delete(editor_);
    editor_=connection_=nullptr;
}
void Badge::event(lv_event_t *e)
{
    auto *self=static_cast<Badge *>(lv_event_get_user_data(e));
    auto *input=lv_indev_active(); if (!input) return;
    auto code=lv_event_get_code(e);
    if (code==LV_EVENT_PRESS_LOST) { self->touch_active_=false; return; }
    if (self->dialog_ || self->editing_) { self->touch_active_=false; return; }
    if (code==LV_EVENT_PRESSED) {
        lv_indev_get_point(input,&self->touch_start_);
        self->touch_distance_=0; self->touch_active_=true; self->touch_long_pressed_=false;
        return;
    }
    if (!self->touch_active_) return;
    lv_point_t point;
    lv_indev_get_point(input,&point);
    int dx=point.x-self->touch_start_.x,dy=point.y-self->touch_start_.y;
    if (std::abs(dx)>self->touch_distance_) self->touch_distance_=std::abs(dx);
    if (std::abs(dy)>self->touch_distance_) self->touch_distance_=std::abs(dy);
    if (code==LV_EVENT_LONG_PRESSED) {
        self->touch_long_pressed_=true;
        return;
    }
    if (code!=LV_EVENT_RELEASED) return;
    self->touch_active_=false;
    // Total displacement also recognizes slow swipes. Handle only the release
    // so LVGL's separate gesture event cannot switch the image a second time.
    if (self->touch_start_.y>=290 && dy<=-50 && -dy>std::abs(dx)) {
        self->back(); return;
    }
    if (std::abs(dx)>=50 && std::abs(dx)>std::abs(dy)) self->direction_=dx<0?1:-1;
    else if (self->touch_distance_<12 && !self->touch_long_pressed_) self->show_dialog();
}
void Badge::action(lv_event_t *e)
{
    auto *self=static_cast<Badge *>(lv_event_get_user_data(e));
    int value=app_ui::action(e);
    if (value==1) self->close_dialog();
    else if (value==2) self->open_editor();
    else if (value==3) self->end_editor();
    else if (value==4 && self->state_.ready && !self->editing_) self->open_editor();
    else if (value==5) self->notifyCoreClosed();
    else if (value==6 || value==7) self->direction_=value==6?-1:1;
}
void Badge::poll(lv_timer_t *t) { static_cast<Badge *>(lv_timer_get_user_data(t))->refresh(); }
bool Badge::back()
{
    if (editing_) { end_editor(); return true; }
    if (dialog_) { close_dialog(); return true; }
    return notifyCoreClosed();
}
bool Badge::pause() { touch_active_=false; end_editor(); if (timer_) lv_timer_pause(timer_); stop(); return true; }
bool Badge::resume()
{
    if (!root_ || !start("badge",6144,2,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)) return false;
    if (timer_) lv_timer_resume(timer_);
    return true;
}
bool Badge::close()
{
    end_editor(); stop();
    touch_active_=false;
    if (timer_) { lv_timer_delete(timer_); timer_=nullptr; }
    if (root_) lv_obj_delete(root_);
    root_=image_obj_=message_=dialog_=nullptr;
    clear_controls();
    welcome_=welcome_button_=welcome_text_=nullptr;
    lv_image_cache_drop(&image_);
    heap_caps_free(display_); heap_caps_free(pending_); display_=pending_=nullptr;
    return true;
}
}
