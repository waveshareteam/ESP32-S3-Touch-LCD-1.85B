#include "Photos.hpp"
#include "esp_jpeg_dec.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>
LV_IMAGE_DECLARE(img_app_gallery);
namespace { constexpr int WIDTH=BSP_LCD_H_RES, HEIGHT=BSP_LCD_V_RES; constexpr size_t PIXELS=WIDTH*HEIGHT,BYTES=PIXELS*2; }
namespace esp_brookesia::apps {
Photos *Photos::instance_;
Photos *Photos::requestInstance() { if(!instance_) instance_=new(std::nothrow) Photos; return instance_; }
Photos::Photos():App("Photos",&img_app_gallery,true,false,false) {}
Photos::~Photos() { close(); if(mutex_) vSemaphoreDelete(mutex_); instance_=nullptr; }
bool Photos::run() {
    if(!mutex_) mutex_=xSemaphoreCreateMutex();
    if(!mutex_) return false;
    wanted_=0; count_=0; shown_=-1; snprintf(message_,sizeof(message_),"Reading SD card...");
    display_pixels_=static_cast<uint16_t *>(heap_caps_aligned_calloc(16,1,BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!display_pixels_) return false;
    for(auto &slot:slots_) {
        slot.index=-1; slot.valid=false;
        slot.pixels=static_cast<uint16_t *>(heap_caps_aligned_alloc(16,BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(!slot.pixels) { close(); return false; }
    }
    root_=app_ui::box(lv_screen_active(),0,0,WIDTH,HEIGHT,0,0);
    lv_obj_add_flag(root_,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(root_,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(root_,event,LV_EVENT_ALL,this);
    image_={}; image_.header.magic=LV_IMAGE_HEADER_MAGIC;
    image_.header.cf=LV_COLOR_FORMAT_RGB565; image_.header.w=WIDTH; image_.header.h=HEIGHT; image_.header.stride=WIDTH*2;
    image_.data_size=BYTES; image_.data=reinterpret_cast<uint8_t *>(display_pixels_);
    image_obj_=lv_image_create(root_); lv_image_set_src(image_obj_,&image_);
    lv_obj_set_pos(image_obj_,0,0);
    lv_obj_remove_flag(image_obj_,LV_OBJ_FLAG_CLICKABLE);
    message_obj_=app_ui::label(root_,"Reading SD card...",56,150,248,&lv_font_montserrat_16,0xffffff);
    lv_obj_set_style_text_align(message_obj_,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_bg_color(message_obj_,lv_color_black(),0);
    lv_obj_set_style_bg_opa(message_obj_,LV_OPA_80,0);
    lv_obj_set_style_pad_all(message_obj_,8,0);
    timer_=lv_timer_create(poll,50,this);
    if(!timer_ || !start("photos",6144,2)) { close(); return false; }
    return true;
}
bool Photos::decode(const char *path,uint16_t *output,char *error,size_t cap) {
    int64_t begin=esp_timer_get_time();
    FILE *f=fopen(path,"rb");
    if(!f) { snprintf(error,cap,"Cannot open JPEG"); return false; }
    bool ok=false; uint8_t *input=nullptr,*decoded=nullptr; jpeg_dec_handle_t decoder=nullptr;
    do {
        if(fseek(f,0,SEEK_END)) break;
        long size=ftell(f);
        if(size<=0 || size>1024*1024 || fseek(f,0,SEEK_SET)) { snprintf(error,cap,"JPEG exceeds 1 MB"); break; }
        input=static_cast<uint8_t *>(heap_caps_aligned_alloc(16,(size+15)&~15,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(!input) { snprintf(error,cap,"Not enough PSRAM"); break; }
        if(fread(input,1,size,f)!=static_cast<size_t>(size) || stopping()) break;
        jpeg_dec_config_t config=DEFAULT_JPEG_DEC_CONFIG(); config.output_type=JPEG_PIXEL_FORMAT_RGB565_LE;
        if(jpeg_dec_open(&config,&decoder)!=JPEG_ERR_OK) break;
        jpeg_dec_io_t io={}; io.inbuf=input; io.inbuf_len=size;
        jpeg_dec_header_info_t info={}; int length=0;
        if(jpeg_dec_parse_header(decoder,&io,&info)!=JPEG_ERR_OK || !info.width || !info.height ||
           jpeg_dec_get_outbuf_len(decoder,&length)!=JPEG_ERR_OK) { snprintf(error,cap,"Unsupported / damaged JPEG"); break; }
        if(length<=0 || length>4*1024*1024 || (size_t)info.width*info.height*2>(size_t)length) {
            snprintf(error,cap,"JPEG too large; use 360 x 360"); break;
        }
        decoded=static_cast<uint8_t *>(heap_caps_aligned_alloc(16,length,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(!decoded) { snprintf(error,cap,"Not enough PSRAM; use 360 x 360"); break; }
        io.outbuf=decoded;
        if(jpeg_dec_process(decoder,&io)!=JPEG_ERR_OK) { snprintf(error,cap,"JPEG decoding failed"); break; }
        if(stopping()) break;
        const auto *src=reinterpret_cast<uint16_t *>(decoded);
        if(info.width==WIDTH && info.height==HEIGHT) {
            // Native-size photographs fill the display with no resampling.
            memcpy(output,src,BYTES);
        } else {
            // Preserve aspect ratio and center-crop to fill the display.
            int crop_w=info.width,crop_h=info.height;
            if((uint32_t)info.width*HEIGHT>(uint32_t)info.height*WIDTH)
                crop_w=std::max(1,(int)((uint32_t)info.height*WIDTH/HEIGHT));
            else
                crop_h=std::max(1,(int)((uint32_t)info.width*HEIGHT/WIDTH));
            const int left=(info.width-crop_w)/2,top=(info.height-crop_h)/2;
            for(int y=0;y<HEIGHT;++y) {
                auto *row=output+y*WIDTH;
                const auto *in=src+(top+(uint32_t)y*crop_h/HEIGHT)*info.width;
                for(int x=0;x<WIDTH;++x) row[x]=in[left+(uint32_t)x*crop_w/WIDTH];
            }
        }
        ok=true;
        ESP_LOGI("photos","%s: %ux%u, %lld ms",strrchr(path,'/')+1,info.width,info.height,(esp_timer_get_time()-begin)/1000);
    } while(false);
    if(!ok && !error[0]) snprintf(error,cap,"SD read / JPEG decode failed");
    if(decoder) jpeg_dec_close(decoder);
    heap_caps_free(decoded); heap_caps_free(input); fclose(f); return ok;
}
void Photos::work() {
    media_files_t files={}; char error[128]={};
    esp_err_t mounted=bsp_sdcard_acquire();
    if(mounted!=ESP_OK) snprintf(error,sizeof(error),"SD unavailable: %s\nSwipe up from bottom to exit",esp_err_to_name(mounted));
    else media_files_scan("/sdcard/photo",".jpg",".jpeg",&files,cancel,this,error,sizeof(error));
    xSemaphoreTake(mutex_,portMAX_DELAY);
    count_=files.count; snprintf(message_,sizeof(message_),"%s",error);
    for(auto &s:slots_) { s.index=-1; s.valid=false; }
    xSemaphoreGive(mutex_);
    while(!stopping() && files.count) {
        int selected=wanted_.load()%files.count;
        int candidates[]={selected,(selected+1)%(int)files.count,(selected+(int)files.count-1)%(int)files.count};
        for(int index:candidates) {
            if(stopping() || wanted_.load()%files.count!=(size_t)selected) break;
            int free_slot=-1; bool exists=false;
            xSemaphoreTake(mutex_,portMAX_DELAY);
            for(int i=0;i<3;++i) if(slots_[i].index==index) exists=true;
            if(!exists) {
                for(int i=0;i<3;++i) if(slots_[i].index<0) { free_slot=i; break; }
                if(free_slot<0) for(int i=0;i<3;++i)
                    if(slots_[i].index!=candidates[0] && slots_[i].index!=candidates[1] && slots_[i].index!=candidates[2]) { free_slot=i; break; }
                if(free_slot>=0) slots_[free_slot].index=-1;
            }
            xSemaphoreGive(mutex_);
            if(exists || free_slot<0) continue;
            char failure[96]={};
            bool decoded=decode(files.paths[index],slots_[free_slot].pixels,failure,sizeof(failure));
            xSemaphoreTake(mutex_,portMAX_DELAY);
            slots_[free_slot].valid=decoded; snprintf(slots_[free_slot].error,sizeof(slots_[free_slot].error),"%s",failure);
            slots_[free_slot].index=index; xSemaphoreGive(mutex_);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    media_files_free(&files); if(mounted==ESP_OK) bsp_sdcard_release();
}
void Photos::refresh() {
    bool changed=false; char message[160]={}; int selected=wanted_.load();
    xSemaphoreTake(mutex_,portMAX_DELAY);
    if(!count_) snprintf(message,sizeof(message),"%s",message_[0]?message_:"No JPEG photos.\nUse /photo on the SD card.");
    else {
        selected%=count_; bool ready=false;
        for(const auto &s:slots_) if(s.index==selected) {
            ready=true;
            if(!s.valid) snprintf(message,sizeof(message),"%s\nSwipe left / right to try another",s.error);
            else if(shown_!=selected) { memcpy(display_pixels_,s.pixels,BYTES); shown_=selected; changed=true; }
            break;
        }
        if(!ready) snprintf(message,sizeof(message),"Loading...");
    }
    xSemaphoreGive(mutex_);
    if(changed) { lv_image_cache_drop(&image_); lv_obj_invalidate(image_obj_); }
    app_ui::text(message_obj_,message);
    if (message[0]) lv_obj_remove_flag(message_obj_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(message_obj_, LV_OBJ_FLAG_HIDDEN);
}
void Photos::event(lv_event_t *e) {
    auto *a=static_cast<Photos *>(lv_event_get_user_data(e)); auto *input=lv_indev_active(); if(!input) return;
    auto code=lv_event_get_code(e);
    if(code==LV_EVENT_PRESSED) lv_indev_get_point(input,&a->touch_start_);
    if(code!=LV_EVENT_GESTURE) return;
    auto dir=lv_indev_get_gesture_dir(input);
    if(dir==LV_DIR_TOP && a->touch_start_.y>=HEIGHT-70) { lv_indev_wait_release(input); a->back(); return; }
    if(dir!=LV_DIR_LEFT && dir!=LV_DIR_RIGHT) return;
    xSemaphoreTake(a->mutex_,portMAX_DELAY); size_t count=a->count_; xSemaphoreGive(a->mutex_);
    if(count) { int i=a->wanted_.load()%count; a->wanted_=(i+(dir==LV_DIR_LEFT?1:(int)count-1))%count; a->refresh(); }
    lv_indev_wait_release(input);
}
void Photos::poll(lv_timer_t *t) { static_cast<Photos *>(lv_timer_get_user_data(t))->refresh(); }
bool Photos::pause() { if(timer_) lv_timer_pause(timer_); stop(); return true; }
bool Photos::resume() { shown_=-1; if(timer_) lv_timer_resume(timer_); return start("photos",6144,2); }
bool Photos::close() {
    stop(); if(timer_) { lv_timer_delete(timer_); timer_=nullptr; }
    if(root_) lv_obj_delete(root_);
    root_=image_obj_=message_obj_=nullptr;
    lv_image_cache_drop(&image_); heap_caps_free(display_pixels_); display_pixels_=nullptr;
    for(auto &s:slots_) { heap_caps_free(s.pixels); s={}; }
    return true;
}
}
