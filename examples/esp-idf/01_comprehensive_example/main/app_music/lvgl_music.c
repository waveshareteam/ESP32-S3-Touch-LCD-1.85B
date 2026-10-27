#include "lvgl_music.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "bsp_board_extra.h"

static const char *TAG = "music";


static lv_obj_t *btn_play_ctrl = NULL;
static lv_obj_t *btn_next = NULL;
static lv_obj_t *btn_last = NULL;
static lv_obj_t *btn_vol = NULL;
static lv_obj_t *btn_menu = NULL;
static lv_obj_t *slider_time = NULL;
static lv_obj_t *label_music_name;
static lv_obj_t *label_music_author;
static char music_buf[100] = {0};
static generic_file_list_t *mp3_file_list = NULL;
static int32_t play_mp3_index = 0;
static lv_timer_t  *timer_1s = NULL;
static uint32_t music_time_count = 0;
static lv_obj_t *label_music_play_time_now;
static lv_obj_t *label_music_play_time;

static void lv_create_menu_msgbox(void);
void ui_set_playing(bool playing);


static void timer_cb(lv_timer_t * t)
{
    if(music_time_count<240)
    {
        music_time_count++;
        lv_slider_set_value(slider_time, music_time_count, LV_ANIM_ON);

        uint8_t minutes = music_time_count / 60; 
        uint8_t seconds = music_time_count % 60;  

        lv_label_set_text_fmt(label_music_play_time_now, "%02d:%02d", minutes, seconds);
    }
    
}

static void play_event_click_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    if(lv_obj_has_state(obj, LV_STATE_CHECKED)) 
    {
        printf("暂停\r\n");
        Audio_Pause_Play();
        lv_timer_pause(timer_1s);
    }
    else 
    {
        printf("开始\r\n");
        esp_asp_state_t status = Audio_Get_Current_State();
        if(status == ESP_ASP_STATE_PAUSED)
        {
            Audio_Resume_Play();
            lv_timer_resume(timer_1s);
        }
        else
        {
            memset(music_buf,0,sizeof(music_buf));
            sprintf(music_buf,"file:/%s",mp3_file_list->list[play_mp3_index]);
            printf("play:%s\r\n",music_buf);
            Audio_Play_Music(music_buf);
            lv_label_set_text_fmt(label_music_author, "%s",mp3_file_list->list[play_mp3_index]+14);

            music_time_count = 0;
            lv_slider_set_value(slider_time, 0, LV_ANIM_ON);
            lv_label_set_text_fmt(label_music_play_time_now, "00:00");
            lv_timer_resume(timer_1s);
        }
    }
}

static void menu_event_click_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        lv_create_menu_msgbox();
    }

}

static void last_event_click_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {

        play_mp3_index--;
        if(play_mp3_index<0)
        {
            play_mp3_index = mp3_file_list->count-1;
        }

        memset(music_buf,0,sizeof(music_buf));
        sprintf(music_buf,"file:/%s",mp3_file_list->list[play_mp3_index]);
        printf("play:%s\r\n",music_buf);
        Audio_Play_Music(music_buf);

        printf("上一首\r\n");
        ui_set_playing(true);
        lv_label_set_text_fmt(label_music_author, "%s",mp3_file_list->list[play_mp3_index]+14);
        music_time_count = 0;
        lv_slider_set_value(slider_time, 0, LV_ANIM_ON);
        lv_label_set_text_fmt(label_music_play_time_now, "00:00");
        lv_timer_resume(timer_1s);
    }

}

static void nest_event_click_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        play_mp3_index++;
        if(play_mp3_index>=mp3_file_list->count)
        {
            play_mp3_index = 0;
        }
        
        memset(music_buf,0,sizeof(music_buf));
        sprintf(music_buf,"file:/%s",mp3_file_list->list[play_mp3_index]);
        printf("play:%s\r\n",music_buf);
        Audio_Play_Music(music_buf);

        printf("下一首\r\n");
        ui_set_playing(true);
        lv_label_set_text_fmt(label_music_author, "%s",mp3_file_list->list[play_mp3_index]+14);
        music_time_count = 0;
        lv_slider_set_value(slider_time, 0, LV_ANIM_ON);
        lv_label_set_text_fmt(label_music_play_time_now, "00:00");
        lv_timer_resume(timer_1s);
    }

}

static void menu_list_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    if(code == LV_EVENT_CLICKED) {

        play_mp3_index = (int32_t)lv_obj_get_index(obj);
        LV_UNUSED(obj);

        memset(music_buf,0,sizeof(music_buf));
        sprintf(music_buf,"file:/%s",mp3_file_list->list[play_mp3_index]);
        printf("play:%s\r\n",music_buf);
        Audio_Play_Music(music_buf);

        ui_set_playing(true);

         lv_label_set_text_fmt(label_music_author, "%s",mp3_file_list->list[play_mp3_index]+14);
        music_time_count = 0;
        lv_slider_set_value(slider_time, 0, LV_ANIM_ON);
        lv_label_set_text_fmt(label_music_play_time_now, "00:00");
        lv_timer_resume(timer_1s);
    }
}


static void lv_create_menu_msgbox(void)
{

    lv_obj_t * setting = lv_msgbox_create(lv_screen_active());

    lv_obj_set_style_clip_corner(setting, true, LV_PART_MAIN);
    lv_obj_set_style_radius(setting, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(setting, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(setting, 200, LV_PART_MAIN);

    lv_obj_set_size(setting, 280, 220); 
    lv_obj_center(setting);

    lv_msgbox_add_title(setting, "Now Playing");
    lv_obj_t * title_bar = lv_msgbox_get_title(setting);

    lv_obj_set_height(title_bar, 48); 
    lv_obj_set_style_pad_top(title_bar, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(title_bar, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x2196F3), LV_PART_MAIN);
    lv_obj_set_style_text_color(title_bar, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_bar, &lv_font_montserrat_20, LV_PART_MAIN);

    lv_obj_t * close_btn = lv_msgbox_add_close_button(setting);
    lv_obj_set_size(close_btn, 36, 36);
    lv_obj_set_style_pad_all(close_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(close_btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_text_font(close_btn, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFF5252), LV_PART_MAIN);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFF1744), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(close_btn, lv_color_white(), LV_PART_MAIN);

    lv_obj_t * content = lv_msgbox_get_content(setting);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_right(content, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_all(content, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_top(content, 10, LV_PART_MAIN);

    lv_obj_set_height(content, 180); 

    lv_obj_t * list = lv_list_create(content);

    lv_obj_set_size(list, lv_pct(100), lv_pct(100)); 
    lv_obj_set_style_radius(list, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(list, lv_color_hex(0xF5F5F5), LV_PART_MAIN);
    lv_obj_set_style_border_color(list, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 5, LV_PART_MAIN);

    for(uint8_t i = 0;i<mp3_file_list->count;i++)
    {
        lv_obj_t * list_item = lv_list_add_btn(list, LV_SYMBOL_AUDIO, mp3_file_list->list[i]);
        lv_obj_set_style_text_font(list_item, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_add_event_cb(list_item, menu_list_event_handler, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_set_style_bg_color(list, lv_color_hex(0xE3F2FD), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(list, lv_color_hex(0x1976D2), LV_STATE_PRESSED);
}



void init_music_ui_screen(lv_obj_t *parent,generic_file_list_t *file_list)
{
    LV_IMAGE_DECLARE(backgroud);
    LV_IMAGE_DECLARE(spectrum);

    lv_obj_t * bg = lv_image_create(parent);
    lv_image_set_src(bg, &backgroud);
    lv_obj_align(bg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF); 
    lv_obj_set_scroll_dir(parent, LV_DIR_NONE); 

    lv_obj_t * st = lv_image_create(bg);
    lv_image_set_src(st, &spectrum);
    lv_obj_align(st, LV_ALIGN_CENTER, 0, 0);

    if(file_list->count == 0)
    {
        return ;
    }
    
    mp3_file_list = file_list;

    LV_IMAGE_DECLARE(play);
    LV_IMAGE_DECLARE(stop);
    LV_IMAGE_DECLARE(last);
    LV_IMAGE_DECLARE(next);
    LV_IMAGE_DECLARE(vol);
    LV_IMAGE_DECLARE(menu);

    /*Create an ctrl button*/
    btn_play_ctrl = lv_imagebutton_create(bg);
    lv_imagebutton_set_src(btn_play_ctrl, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &stop, NULL);
    lv_imagebutton_set_src(btn_play_ctrl, LV_IMAGEBUTTON_STATE_PRESSED, NULL, &stop, NULL);
    lv_imagebutton_set_src(btn_play_ctrl, LV_IMAGEBUTTON_STATE_CHECKED_RELEASED, NULL, &play, NULL);
    lv_imagebutton_set_src(btn_play_ctrl, LV_IMAGEBUTTON_STATE_CHECKED_PRESSED, NULL, &play, NULL);
    lv_obj_align(btn_play_ctrl, LV_ALIGN_CENTER, 0, 50);
    lv_obj_add_flag(btn_play_ctrl, LV_OBJ_FLAG_CHECKABLE);
    lv_imagebutton_set_state(btn_play_ctrl, LV_IMAGEBUTTON_STATE_CHECKED_RELEASED);
    lv_obj_set_style_bg_color(btn_play_ctrl, lv_color_hex(0x444444), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_play_ctrl, 8, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn_play_ctrl, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_play_ctrl, play_event_click_cb, LV_EVENT_CLICKED, NULL);

    btn_last = lv_image_create(bg);
    lv_image_set_src(btn_last, &last); 
    lv_obj_align(btn_last,LV_ALIGN_CENTER,-80,50);
    lv_obj_add_flag(btn_last, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_last, last_event_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_last, lv_color_hex(0x444444), LV_STATE_PRESSED); 
    lv_obj_set_style_radius(btn_last, 8, LV_STATE_PRESSED); 
    lv_obj_set_style_bg_opa(btn_last, LV_OPA_70, LV_STATE_PRESSED); 

    btn_next = lv_image_create(bg);
    lv_image_set_src(btn_next, &next); 
    lv_obj_align(btn_next,LV_ALIGN_CENTER,80,50);
    lv_obj_add_flag(btn_next, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_next, nest_event_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0x444444), LV_STATE_PRESSED); 
    lv_obj_set_style_radius(btn_next, 8, LV_STATE_PRESSED); 
    lv_obj_set_style_bg_opa(btn_next, LV_OPA_70, LV_STATE_PRESSED); 

    slider_time = lv_slider_create(bg);
    lv_obj_center(slider_time);
    lv_slider_set_range(slider_time, 0, 100);
    lv_obj_set_style_anim_duration(slider_time, 2000, 0);
    lv_obj_set_style_bg_opa(slider_time, LV_OPA_0, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider_time, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_time, lv_color_hex(0xC0C0C0), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_time, lv_color_hex(0x8B4513), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider_time, LV_OPA_80, LV_PART_INDICATOR);
    lv_obj_set_size(slider_time, 230,5);

    /*Create a label below the slider*/
    label_music_play_time_now = lv_label_create(bg);
    lv_label_set_text_fmt(label_music_play_time_now, "00:00");
    lv_obj_align_to(label_music_play_time_now, slider_time, LV_ALIGN_OUT_BOTTOM_MID, -100, 10);

    label_music_play_time = lv_label_create(bg);
    lv_label_set_text_fmt(label_music_play_time, "03:58");
    lv_obj_align_to(label_music_play_time, slider_time, LV_ALIGN_OUT_BOTTOM_MID, 100, 10);

    btn_menu = lv_image_create(bg);
    lv_image_set_src(btn_menu, &menu); 
    lv_obj_align_to(btn_menu, slider_time,LV_ALIGN_OUT_TOP_RIGHT, 0, -10);
    lv_obj_add_flag(btn_menu, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_menu, menu_event_click_cb, LV_EVENT_CLICKED, NULL);

    label_music_author = lv_label_create(bg);
    lv_label_set_long_mode(label_music_author, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label_music_author, " ");
    lv_obj_set_width(label_music_author, lv_obj_get_width(slider_time));
    lv_obj_set_style_text_align(label_music_author, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_label_set_recolor(label_music_author, false);

    lv_obj_set_x(label_music_author, lv_obj_get_x(slider_time));

    lv_obj_set_y(label_music_author, lv_obj_get_y(slider_time) - 80); 

    lv_obj_set_style_text_font(label_music_author, &lv_font_montserrat_22, LV_STATE_DEFAULT);

    label_music_name = lv_label_create(bg);
    lv_label_set_text_fmt(label_music_name," ");
    lv_obj_align_to(label_music_name, label_music_author,LV_ALIGN_OUT_TOP_MID, 0, -10);
    lv_obj_set_style_text_font(label_music_name, &lv_font_montserrat_26, 0);

    timer_1s = lv_timer_create(timer_cb, 1000, NULL);
    lv_timer_pause(timer_1s);

}

void ui_set_playing(bool playing)
{
    if (playing == true) {
        lv_timer_resume(timer_1s);
        lv_imagebutton_set_state(btn_play_ctrl, LV_IMAGEBUTTON_STATE_RELEASED);
    } else {
        lv_imagebutton_set_state(btn_play_ctrl, LV_IMAGEBUTTON_STATE_CHECKED_RELEASED);
        lv_timer_pause(timer_1s);
    }
}