#include "bsp_board_extra.h"
#include "Music.hpp"
#include "esp_mp3_dec.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>
LV_IMAGE_DECLARE(img_app_musicplayer);
LV_IMAGE_DECLARE(img_app_music_cover_1);
using namespace app_ui;
namespace esp_brookesia::apps {
Music *Music::instance_;
Music *Music::requestInstance() { if(!instance_) instance_=new(std::nothrow) Music; return instance_; }
Music::Music():App("Music",&img_app_musicplayer,true,false,false) {}
Music::~Music() { close(); instance_=nullptr; }
bool Music::run() {
    root_=screen("Now Playing",event,this);
    count_=label(root_,"0 / 0",132,76,96,&lv_font_montserrat_12,MUTED);
    lv_obj_set_style_text_align(count_,LV_TEXT_ALIGN_CENTER,0);
    auto *cover=lv_image_create(root_); lv_image_set_src(cover,&img_app_music_cover_1);
    lv_image_set_pivot(cover,0,0); lv_image_set_scale(cover,128);
    lv_obj_set_pos(cover,(BSP_LCD_H_RES-img_app_music_cover_1.header.w*128/256)/2,98);
    title_=label(root_,"Local music",56,208,248,&lv_font_source_han_sans_sc_16_cjk);
    lv_obj_set_style_text_align(title_,LV_TEXT_ALIGN_CENTER,0); lv_label_set_long_mode(title_,LV_LABEL_LONG_MODE_DOTS);
    state_=label(root_,"Reading SD card...",56,234,248,&lv_font_montserrat_12,MUTED);
    lv_obj_set_style_text_align(state_,LV_TEXT_ALIGN_CENTER,0);
    progress_=lv_bar_create(root_); lv_obj_set_pos(progress_,70,258); lv_obj_set_size(progress_,220,5); lv_bar_set_range(progress_,0,1000);
    lv_obj_set_style_bg_color(progress_,lv_color_hex(0xdce4f1),0); lv_obj_set_style_bg_color(progress_,lv_color_hex(BLUE),LV_PART_INDICATOR);
    elapsed_=label(root_,"00:00",70,270,70,&lv_font_montserrat_12,MUTED);
    duration_=label(root_,"00:00",220,270,70,&lv_font_montserrat_12,MUTED); lv_obj_set_style_text_align(duration_,LV_TEXT_ALIGN_CENTER,0);
    button(root_,LV_SYMBOL_PREV,82,296,48,38,1,event,this);
    auto *play=button(root_,LV_SYMBOL_PLAY,152,290,56,48,2,event,this,true); play_label_=lv_obj_get_child(play,0);
    button(root_,LV_SYMBOL_NEXT,230,296,48,38,3,event,this);
    play_=false; selected_=0; view_=View{};
    timer_=lv_timer_create(poll,200,this);
    if(!timer_ || !start("music",8192,4)) { close(); return false; }
    return true;
}
void Music::work() {
    music_files_t files={}; View v;
    auto mounted=bsp_sdcard_acquire();
    if(mounted!=ESP_OK) { snprintf(v.status,sizeof(v.status),"SD unavailable: %s",esp_err_to_name(mounted)); publish(v); return; }
    auto scanned=music_scan("/sdcard/music",&files,cancel,this);
    if(scanned!=MUSIC_OK) {
        snprintf(v.status,sizeof(v.status),scanned==MUSIC_EMPTY?"No MP3 files in /music":scanned==MUSIC_NO_FOLDER?"Create /music on the SD card":"SD scan failed");
        publish(v); bsp_sdcard_release(); return;
    }
    v.count=files.count;
    auto *compressed=static_cast<uint8_t *>(heap_caps_malloc(2048,MALLOC_CAP_8BIT));
    auto *pcm=static_cast<uint8_t *>(heap_caps_malloc(8192,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    FILE *input=nullptr; void *decoder=nullptr; bool audio=false;
    unsigned last=UINT32_MAX; uint64_t samples=0; uint32_t rate=0; uint8_t channels=0;
    auto cleanup=[&]() {
        if(audio) { bsp_extra_audio_drain(); bsp_extra_codec_dev_stop(); bsp_extra_audio_release(); audio=false; }
        if(decoder) { esp_mp3_dec_close(decoder); decoder=nullptr; }
        if(input) { fclose(input); input=nullptr; }
        rate=0; channels=0;
    };
    if(!compressed || !pcm) {
        snprintf(v.status,sizeof(v.status),"Not enough memory for audio"); publish(v);
    } else while(!stopping()) {
        unsigned index=selected_.load()%files.count;
        if(index!=last) {
            cleanup(); last=index; samples=0; v.position=v.duration=0; v.ready=v.playing=false; v.index=index;
            music_title(files.paths[index],v.title,sizeof(v.title));
            snprintf(v.status,sizeof(v.status),"Reading MP3 information..."); publish(v);
            music_info_t info={}; auto probe=music_probe(files.paths[index],&info,cancel,this);
            if(stopping()) break;
            if(probe!=MUSIC_OK) { snprintf(v.status,sizeof(v.status),"Invalid MP3; select another track"); play_=false; }
            else { v.duration=info.duration_ms; v.ready=true; snprintf(v.status,sizeof(v.status),"Ready"); }
            publish(v);
        }
        if(!v.ready || !play_.load()) {
            if(audio) { bsp_extra_audio_drain(); bsp_extra_codec_dev_stop(); bsp_extra_audio_release(); audio=false; rate=0; }
            if(v.ready && v.playing) snprintf(v.status,sizeof(v.status),"Paused");
            v.playing=false;
            publish(v); vTaskDelay(pdMS_TO_TICKS(30)); continue;
        }
        if(!input) {
            input=fopen(files.paths[index],"rb");
            if(!input || !music_skip_id3(input) || esp_mp3_dec_open(nullptr,0,&decoder)!=ESP_AUDIO_ERR_OK) {
                cleanup(); play_=false; v.ready=false; snprintf(v.status,sizeof(v.status),"Cannot open MP3 decoder"); publish(v); continue;
            }
            samples=0; v.position=0;
            setvbuf(input,nullptr,_IOFBF,8192);
        }
        int size=0; bool failed=false,eof=false;
        if(fread(compressed,1,4,input)!=4) { eof=!ferror(input); failed=!eof; }
        else {
            unsigned searched=0;
            while(!(size=music_frame_bytes(compressed)) && !stopping()) {
                if(!memcmp(compressed,"TAG",3) || !memcmp(compressed,"APET",4)) { eof=true; break; }
                int c=fgetc(input); if(c==EOF) { eof=!ferror(input); failed=!eof; break; }
                memmove(compressed,compressed+1,3); compressed[3]=(uint8_t)c;
                if(++searched>65536) { failed=true; break; }
            }
            if(size>0 && fread(compressed+4,1,size-4,input)!=(size_t)size-4) failed=true;
        }
        if(eof || failed || stopping()) {
            cleanup(); play_=false; v.playing=false;
            if(eof) { v.position=v.duration; snprintf(v.status,sizeof(v.status),"Finished"); }
            else { v.ready=false; snprintf(v.status,sizeof(v.status),"SD read / MP3 frame error"); }
            publish(v); continue;
        }
        esp_audio_dec_in_raw_t raw={}; raw.buffer=compressed; raw.len=size;
        esp_audio_dec_out_frame_t frame={}; frame.buffer=pcm; frame.len=8192;
        esp_audio_dec_info_t info={};
        auto result=esp_mp3_dec_decode(decoder,&raw,&frame,&info);
        if(result!=ESP_AUDIO_ERR_OK || raw.consumed!=raw.len) {
            ESP_LOGW("music","MP3 decode error: %d",(int)result);
            cleanup(); play_=false; v.ready=false; snprintf(v.status,sizeof(v.status),"MP3 decode failed; try another track"); publish(v); continue;
        }
        if(!frame.decoded_size) continue;
        if(info.bits_per_sample!=16 || !info.sample_rate || (info.channel!=1 && info.channel!=2)) {
            cleanup(); play_=false; v.ready=false; snprintf(v.status,sizeof(v.status),"Unsupported PCM format"); publish(v); continue;
        }
        // ES8311 drives one mono speaker: preserve both channels of stereo MP3s.
        size_t frame_samples=frame.decoded_size/(2*info.channel);
        if(info.channel==2) {
            auto *s=reinterpret_cast<int16_t *>(pcm);
            for(size_t i=0;i<frame_samples;++i) s[i]=(int32_t(s[2*i])+s[2*i+1])/2;
        }
        if(!audio) audio=bsp_extra_audio_acquire(BSP_EXTRA_AUDIO_OWNER_MUSIC, 100);
        esp_err_t err=audio?ESP_OK:ESP_ERR_TIMEOUT;
        if(err==ESP_OK && (rate!=info.sample_rate || channels!=1)) {
            err=bsp_extra_codec_set_fs(info.sample_rate, 16, static_cast<i2s_slot_mode_t>(1)); rate=info.sample_rate; channels=1;
        }
        if(err==ESP_OK) err=bsp_extra_i2s_write(pcm, frame_samples*2, nullptr, 1000);
        if(err!=ESP_OK) {
            cleanup(); play_=false; v.ready=false; snprintf(v.status,sizeof(v.status),"Audio unavailable: %s",esp_err_to_name(err)); publish(v); continue;
        }
        samples+=frame_samples;
        v.position=std::min<uint64_t>(samples*1000/info.sample_rate,v.duration);
        v.playing=true; snprintf(v.status,sizeof(v.status),"Playing from SD card"); publish(v);
        // I2S write paces playback. All codec calls execute on this same worker.
    }
    cleanup(); heap_caps_free(compressed); heap_caps_free(pcm); music_files_free(&files); bsp_sdcard_release();
}
void Music::refresh() {
    View v; portENTER_CRITICAL(&lock_); v=view_; portEXIT_CRITICAL(&lock_);
    text(title_,v.title); text(state_,v.status); text(play_label_,v.playing?LV_SYMBOL_PAUSE:LV_SYMBOL_PLAY);
    char s[32]; snprintf(s,sizeof(s),"%02u / %02u",v.count?v.index+1:0,v.count); text(count_,s);
    music_format_time(v.position,s,sizeof(s)); text(elapsed_,s);
    music_format_time(v.duration,s,sizeof(s)); text(duration_,s);
    lv_bar_set_value(progress_,v.duration?(uint64_t)v.position*1000/v.duration:0,LV_ANIM_OFF);
}
void Music::event(lv_event_t *e) {
    auto *a=static_cast<Music *>(lv_event_get_user_data(e)); int id=action(e);
    if(id==0) { a->back(); return; }
    View v; portENTER_CRITICAL(&a->lock_); v=a->view_; portEXIT_CRITICAL(&a->lock_);
    if(id==2) {
        if(v.ready) a->play_=!a->play_.load();
        else {
            a->stop(); a->play_=false;
            v.playing=false; snprintf(v.status,sizeof(v.status),"Reading SD card..."); a->publish(v);
            if(!a->start("music",8192,4)) { snprintf(v.status,sizeof(v.status),"Not enough memory. Reopen to retry."); a->publish(v); }
        }
    }
    else if(v.count) a->selected_=(a->selected_.load()+(id==1?v.count-1:1))%v.count;
}
void Music::poll(lv_timer_t *t) { static_cast<Music *>(lv_timer_get_user_data(t))->refresh(); }
bool Music::pause() { play_=false; if(timer_) lv_timer_pause(timer_); stop(); return true; }
bool Music::resume() { play_=false; if(timer_) lv_timer_resume(timer_); return start("music",8192,4); }
bool Music::close() { play_=false; stop(); if(timer_) { lv_timer_delete(timer_); timer_=nullptr; } if(root_) lv_obj_delete(root_); root_=nullptr; return true; }
}
