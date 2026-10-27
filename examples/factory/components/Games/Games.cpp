// Reuses the rule engine from the completed GUI Guider/RLCD 2048 port.
#include "Games.hpp"
#include "app_support.hpp"
#include "esp_random.h"
#include <cstdio>
#include <new>
LV_IMAGE_DECLARE(img_app_2048);
using namespace app_ui;
namespace {
esp_brookesia::systems::phone::App::Config game_config()
{
    auto cfg=esp_brookesia::systems::phone::App::Config::SIMPLE_CONSTRUCTOR(&img_app_2048,false,false);
    cfg.flags.enable_navigation_gesture=false;
    return cfg;
}
}
namespace esp_brookesia::apps {
Games *Games::instance_;
Games *Games::requestInstance() { if(!instance_) instance_=new(std::nothrow) Games; return instance_; }
Games::Games():App(systems::base::App::Config::SIMPLE_CONSTRUCTOR("2048",&img_app_2048,true),game_config()) {}
Games::~Games() { close(); instance_=nullptr; }
bool Games::run() {
    root_=screen("2048",event,this);
    button(root_,LV_SYMBOL_REFRESH,248,40,36,36,1,event,this,true);
    auto *a=box(root_,76,82,100,28),*b=box(root_,184,82,100,28);
    label(a,"SCORE",6,8,40,&lv_font_montserrat_12,MUTED);
    label(b,"BEST",6,8,40,&lv_font_montserrat_12,MUTED);
    score_=label(a,"0",46,6,48,&lv_font_montserrat_14);
    best_=label(b,"0",46,6,48,&lv_font_montserrat_14);
    lv_obj_set_style_text_align(score_,LV_TEXT_ALIGN_RIGHT,0);
    lv_obj_set_style_text_align(best_,LV_TEXT_ALIGN_RIGHT,0);
    auto *board=box(root_,76,114,208,208,0xb7ada1,8);
    lv_obj_add_flag(board,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(board,LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(board,gesture,LV_EVENT_GESTURE,this);
    for(int i=0;i<16;++i) {
        cells_[i]=box(board,6+(i%4)*50,6+(i/4)*50,46,46,0xcdc2b5,6);
        numbers_[i]=label(cells_[i],"",0,0,46,&lv_font_montserrat_20);
        lv_obj_set_style_text_align(numbers_[i],LV_TEXT_ALIGN_CENTER,0); lv_obj_center(numbers_[i]);
    }
    hint_=label(root_,"Swipe to merge",110,330,140,&lv_font_montserrat_12,MUTED);
    lv_obj_set_style_text_align(hint_,LV_TEXT_ALIGN_CENTER,0);
    bool empty=true;
    for(const auto &row:game_.cells) for(auto n:row) if(n) empty=false;
    if(empty) reset(); else refresh();
    return true;
}
void Games::reset() { game_2048_reset(&game_); for(int i=0;i<2;++i) game_2048_spawn(&game_,esp_random(),esp_random()); refresh(); }
void Games::refresh() {
    static const uint32_t colors[]={0xcdc2b5,0xeee4da,0xede0c8,0xf2b179,0xf59563,0xf67c5f,0xf65e3b,0xedcf72,0xedcc61,0xedc850,0xedc53f,0xedc22e};
    char s[24];
    snprintf(s,sizeof(s),"%lu",(unsigned long)game_.score); text(score_,s);
    snprintf(s,sizeof(s),"%lu",(unsigned long)game_.best); text(best_,s);
    for(int i=0;i<16;++i) {
        uint32_t n=game_.cells[i/4][i%4]; unsigned exponent=0; for(uint32_t v=n;v>1;v>>=1) ++exponent;
        lv_obj_set_style_bg_color(cells_[i],lv_color_hex(colors[exponent<12?exponent:11]),0);
        lv_obj_set_style_text_color(numbers_[i],lv_color_hex(n<=4?0x776e65:0xffffff),0);
        lv_obj_set_style_text_font(numbers_[i],n>=16384?&lv_font_montserrat_12:n>=1024?&lv_font_montserrat_18:&lv_font_montserrat_20,0);
        if(n) snprintf(s,sizeof(s),"%lu",(unsigned long)n); else s[0]=0;
        text(numbers_[i],s); lv_obj_center(numbers_[i]);
    }
    text(hint_,game_.won?"2048! You win.":game_.over?"No moves. Try again!":"Swipe to merge");
}
void Games::gesture(lv_event_t *e) {
    auto *app=static_cast<Games *>(lv_event_get_user_data(e)); auto *input=lv_indev_active(); if(!input) return;
    auto dir=lv_indev_get_gesture_dir(input); gg_2048_move_direction move;
    switch(dir) { case LV_DIR_LEFT: move=GG_2048_MOVE_LEFT; break; case LV_DIR_RIGHT: move=GG_2048_MOVE_RIGHT; break;
    case LV_DIR_TOP: move=GG_2048_MOVE_UP; break; case LV_DIR_BOTTOM: move=GG_2048_MOVE_DOWN; break; default:return; }
    lv_indev_wait_release(input);
    if(game_2048_slide(&app->game_,move)) game_2048_spawn(&app->game_,esp_random(),esp_random());
    app->refresh();
}
void Games::event(lv_event_t *e) { auto *a=static_cast<Games *>(lv_event_get_user_data(e)); if(action(e)==0) a->back(); else a->reset(); }
bool Games::close() { if(root_) lv_obj_delete(root_); root_=nullptr; return true; }
}
