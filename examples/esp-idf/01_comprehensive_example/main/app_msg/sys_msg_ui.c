#include "sys_msg_ui.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"

#include "bsp_board_extra.h"
#include "pcf85063a.h"
#include "submenu_ui/set_lv_style.h"
#include "submenu_ui/set_about.h"
#include "submenu_ui/set_wifi_service.h"
#include "submenu_ui/set_bat_msg.h"
#include "wake_word_det/wake_word_drv.h"

LV_IMG_DECLARE(arrow);  
LV_IMG_DECLARE(backlights);  
LV_IMG_DECLARE(battery);  
LV_IMG_DECLARE(info);  
LV_IMG_DECLARE(sound);  
LV_IMG_DECLARE(WIFI);  

static lv_obj_t *g_msg_ui_list;
static lv_obj_t *main_msg_screen; 
static lv_obj_t *wifi_screen;  
static lv_obj_t *about_screen;  
static lv_obj_t *bat_screen;
static lv_obj_t *rtc_label = NULL;
static lv_obj_t *tf_label = NULL;
static pcf85063a_dev_t s_rtc_dev;

static void add_button_with_arrow(lv_obj_t *list,const void *icon, const char *text, lv_event_cb_t cb, void *user_data);
static void add_button_with_slider(lv_obj_t *list,const void *icon, const char *text, lv_event_cb_t cb, void *user_data);
static lv_obj_t *add_button_with_label(lv_obj_t *list,const void *icon, const char *text);

static void slider_value_change_sound_vol_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    uint32_t cur_val = lv_slider_get_value(slider);

    lv_obj_t *label = lv_obj_get_child(slider, 0);
    lv_label_set_text_fmt(label, "%ld", cur_val);
    lv_obj_center(label); 

    Volume_Adjustment(cur_val);
}

static void slider_value_change_brightness_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    uint32_t cur_val = lv_slider_get_value(slider);

    lv_obj_t *label = lv_obj_get_child(slider, 0);
    lv_label_set_text_fmt(label, "%ld", cur_val);
    lv_obj_center(label); 

    bsp_display_brightness_set(cur_val);
}

static void wifi_btn_click_cb(lv_event_t *e)
{
    lv_screen_load(wifi_screen); 
}

static void bat_msg_btn_click_cb(lv_event_t *e)
{
    lv_screen_load(bat_screen);  
}

static void about_btn_click_cb(lv_event_t *e)
{
    lv_screen_load(about_screen); 
}

static void update_timer_cb(lv_timer_t *t)
{
    pcf85063a_datetime_t Now_time;
    pcf85063a_get_time_date(&s_rtc_dev, &Now_time);
    lv_label_set_text_fmt(rtc_label, "%d:%d:%d",Now_time.hour,Now_time.min,Now_time.sec);
}

esp_err_t msg_driver_init(void)
{
    esp_err_t ret = ESP_OK;
    bq27220_drv_init();
    
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_get_bus_handle(BSP_I2C_NUM,&i2c_bus);

    pcf85063a_init(&s_rtc_dev, i2c_bus, PCF85063A_ADDRESS);

    wifi_init_sta();
    return ret;
} 

void msg_ui_screen_init(lv_obj_t *main_scr_tmp,lv_obj_t *parent)
{
    main_msg_screen = main_scr_tmp; 
    g_msg_ui_list = lv_list_create(parent);
    if(g_msg_ui_list == NULL) return; 

    set_screen_styles_t *styles = get_set_screen_styles();
    lv_obj_center(g_msg_ui_list);
    lv_obj_add_style(g_msg_ui_list, &styles->list, 0);
    lv_obj_set_size(g_msg_ui_list, 320, 320); 
    lv_obj_set_scrollbar_mode(g_msg_ui_list, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_style_pad_top(g_msg_ui_list, 8, 0);
    lv_obj_set_style_pad_bottom(g_msg_ui_list, 8, 0);
    lv_obj_set_scroll_dir(g_msg_ui_list, LV_DIR_VER); 
    lv_obj_set_scroll_snap_y(g_msg_ui_list, LV_SCROLL_SNAP_CENTER);
    
    //
    lv_list_add_text(g_msg_ui_list, "Wireless");
    add_button_with_arrow(g_msg_ui_list, &WIFI, "Wifi", wifi_btn_click_cb, NULL);

    lv_list_add_text(g_msg_ui_list, "Media");
    add_button_with_slider(g_msg_ui_list, &sound, "Sound", slider_value_change_sound_vol_cb, NULL);

    lv_list_add_text(g_msg_ui_list, "More");
    add_button_with_slider(g_msg_ui_list, &backlights, "Backlight", slider_value_change_brightness_cb, NULL);
    add_button_with_arrow(g_msg_ui_list, &battery, "Battery", bat_msg_btn_click_cb, NULL);

    rtc_label = add_button_with_label(g_msg_ui_list, &info, "RTC Time");
    lv_timer_create(update_timer_cb, 1000, NULL);

    tf_label = add_button_with_label(g_msg_ui_list, &info, "TF Card");
    sdmmc_card_t *sdcard = bsp_sdcard_get_handle();
    if (sdcard != NULL)
    {
 
        uint64_t total_bytes = (uint64_t)sdcard->csd.capacity * sdcard->csd.sector_size;
        uint32_t total_mb = (uint32_t)(total_bytes / (1024 * 1024));
        lv_label_set_text_fmt(tf_label, "%lu MB",total_mb); 
    }

    add_button_with_arrow(g_msg_ui_list, &info, "About", about_btn_click_cb, NULL);

    wifi_screen = lv_obj_create(NULL);
    bat_screen = lv_obj_create(NULL);
    about_screen = lv_obj_create(NULL);

    init_wifi_service_screen(wifi_screen,main_msg_screen);
    init_bat_settings_screen(bat_screen,main_msg_screen);
    init_about_settings_screen(about_screen,main_msg_screen);
    
    
}


/**
 * @brief Create a list button with a right-side arrow icon
 * 
 * This function creates a list button object with specified icon, text, and a right-aligned arrow icon.
 * It applies pre-defined styles (default/pressed/hovered) and binds click callback if provided.
 * 
 * @param icon      Pointer to LVGL image descriptor (LV_IMG_DECLARE) for the left icon, NULL for no icon
 * @param text      Text string to display on the button (UTF-8 supported), must be non-NULL
 * @param cb        Event callback function for LV_EVENT_CLICKED event, NULL for no callback
 * @param user_data User-defined data to pass to the callback function, NULL for no user data
 * @return          None (void)
 * 
 * @note Performs null pointer checks to prevent runtime crashes
 */
static void add_button_with_arrow(lv_obj_t *list,const void *icon, const char *text, lv_event_cb_t cb, void *user_data)
{
    // Guard clause: Return early if global list object is not initialized
    if(list == NULL) return; 
    
    set_screen_styles_t *styles = get_set_screen_styles();

    // 1. Create list button object
    lv_obj_t *btn = lv_list_add_button(list, icon, text); 
    if(btn == NULL) return;

    // 2. Apply style configurations to button
    lv_obj_add_style(btn, &styles->list_btn, 0);
    lv_obj_add_style(btn, &styles->list_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(btn, &styles->list_btn_hover, LV_STATE_HOVERED);
    
    // 3. Bind click event callback if provided
    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }

    // 4. Create and configure right-side arrow icon
    lv_obj_t *arrow_img = lv_img_create(btn);
    lv_img_set_src(arrow_img, &arrow);
    lv_obj_set_style_img_recolor(arrow_img, lv_color_hex(0x0D6EFD), LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(arrow_img, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_align(arrow_img, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_size(arrow_img, 16, 16);
}


/**
 * @brief Create a list button with an embedded slider component
 * 
 * This function creates a non-clickable list button containing a slider on the right side.
 * The slider is pre-configured with a range of 1-100, default value of 100, and custom styling.
 * A numeric label displaying the current slider value is centered on the slider knob.
 * 
 * @param icon      Pointer to LVGL image descriptor (LV_IMG_DECLARE) for the left icon, NULL for no icon
 * @param text      Text string to display on the button (UTF-8 supported), must be non-NULL
 * @param cb        Event callback function for LV_EVENT_CLICKED event (unused as button is non-clickable), NULL for no callback
 * @param user_data User-defined data to pass to the callback function, NULL for no user data
 * @return          None (void)
 * 
 * @note The button is set as non-clickable (LV_OBJ_FLAG_CLICKABLE removed) to prioritize slider interaction
 */
static void add_button_with_slider(lv_obj_t *list,const void *icon, const char *text, lv_event_cb_t cb, void *user_data)
{
    if(list == NULL) return;
    
    set_screen_styles_t *styles = get_set_screen_styles();

    // 1. Create list button object
    lv_obj_t *btn = lv_list_add_button(list, icon, text); 
    if(btn == NULL) return;
    
    lv_obj_add_style(btn, &styles->list_btn, LV_STATE_DEFAULT);
    lv_obj_add_style(btn, &styles->list_btn_pressed, LV_STATE_PRESSED);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_CLICKABLE); // Disable button clickability

    // 2. Create slider container
    lv_obj_t *slider_cont = lv_obj_create(btn);
    lv_obj_set_size(slider_cont, 130, 30);
    lv_obj_align(slider_cont, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_style_bg_opa(slider_cont, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(slider_cont, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(slider_cont, 0, LV_STATE_DEFAULT);

    // 3. Create slider component
    lv_obj_t *slider = lv_slider_create(slider_cont);
    lv_obj_set_size(slider, 100, 25);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
    lv_slider_set_range(slider, 1, 100);
    lv_slider_set_value(slider, 100, LV_ANIM_OFF);

    // 4. Configure slider styling
    lv_obj_set_style_radius(slider, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR | LV_STATE_DEFAULT); // Fix: KNOB changed to INDICATOR (LVGL9)
    lv_obj_set_style_shadow_width(slider, 2, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(slider, lv_color_hex(0xCC8800), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(slider, LV_OPA_30, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    // 5. Create numeric label for slider value
    lv_obj_t *slider_label = lv_label_create(slider);
    lv_label_set_text_fmt(slider_label, "%ld", lv_slider_get_value(slider));
    lv_obj_set_style_text_color(slider_label, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(slider_label, &lv_font_montserrat_18, LV_STATE_DEFAULT);
    lv_obj_center(slider_label);

    // 6. Bind callback and user data (if provided)
    if (cb != NULL) {
        lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
}

/**
 * @brief Create a list button with a right-aligned label component
 * 
 * This function creates a clickable list button with a left icon/text and an empty right-aligned label.
 * The right label is pre-styled with black color, montserrat 22 font, and 10px left margin from the button edge.
 * Returns the right label object to allow dynamic text updates after creation.
 * 
 * @param icon  Pointer to LVGL image descriptor (LV_IMG_DECLARE) for the left icon, NULL for no icon
 * @param text  Text string to display on the left side of the button (UTF-8 supported), must be non-NULL
 * @return      Pointer to the created right-side label object (lv_obj_t *) | NULL if creation fails (e.g., invalid list object)
 * 
 * @note Uses LVGL9 API (lv_list_add_btn instead of lv_list_add_button)
 */
static lv_obj_t *add_button_with_label(lv_obj_t *list,const void *icon, const char *text)
{
    if(list == NULL) return NULL;
    
    set_screen_styles_t *styles = get_set_screen_styles();
    
    // 1. Create list button object (LVGL9 API fix: lv_list_add_btn)
    lv_obj_t *btn = lv_list_add_btn(list, icon, text); 
    if(btn == NULL) return NULL;
    
    lv_obj_add_style(btn, &styles->list_btn, LV_STATE_DEFAULT);
    lv_obj_add_style(btn, &styles->list_btn_pressed, LV_STATE_PRESSED);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_CLICKABLE); // Enable button clickability

    // 2. Create right-aligned label component
    lv_obj_t *char_label = lv_label_create(btn);
    lv_label_set_text(char_label, " "); // Initialize with empty space
    lv_obj_set_style_text_color(char_label, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(char_label, &lv_font_montserrat_22, LV_STATE_DEFAULT);
    lv_obj_align(char_label, LV_ALIGN_RIGHT_MID, -10, 0); // Align to right middle (10px left margin)

    return char_label;
}

