#include "set_about.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_idf_version.h"
#include "set_lv_style.h"


static lv_obj_t *return_screen = NULL;
static lv_obj_t *about_screen = NULL;
static lv_obj_t *back_btn = NULL; 

static void back_btn_click_event_cb(lv_event_t *e);
static char *get_mac_address(void);
static void init_about_lv_style(void);

static lv_style_t style_about_title;   
static lv_style_t style_about_label;   
static lv_style_t style_about_value;   


void init_about_settings_screen(lv_obj_t *parent, lv_obj_t *ret_scr)
{
    return_screen = ret_scr;

    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), LV_PART_MAIN);  
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    set_screen_styles_t *styles = get_set_screen_styles();
    init_about_lv_style();

    about_screen = lv_obj_create(parent);
    lv_obj_set_size(about_screen, 360, 280);
    lv_obj_set_style_bg_color(about_screen, lv_color_hex(0xFAFAFA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(about_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(about_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(about_screen, 16, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(about_screen, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(about_screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(about_screen, LV_OPA_10, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(about_screen, 4, LV_PART_MAIN);
    lv_obj_center(about_screen);

    back_btn = lv_button_create(about_screen);
    lv_obj_add_style(back_btn, &styles->back_btn, LV_PART_MAIN);
    lv_obj_add_style(back_btn, &styles->back_btn_pressed, LV_STATE_PRESSED);

    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 45, 0);  
    lv_obj_add_event_cb(back_btn, back_btn_click_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_center(back_icon);

    lv_obj_t *title_label = lv_label_create(about_screen);
    lv_label_set_text(title_label, "Device information");
    lv_obj_add_style(title_label, &style_about_title, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 55);

    #define CREATE_ABOUT_ROW(about_screen, y_offset, label_text, value_text) do { \
        lv_obj_t *row = lv_obj_create(about_screen); \
        lv_obj_set_size(row, lv_pct(90), LV_SIZE_CONTENT); \
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y_offset); \
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW); \
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER); \
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0); \
        lv_obj_set_style_border_width(row, 0, 0); \
        lv_obj_set_style_pad_hor(row, 10, 0); \
        lv_obj_set_style_pad_ver(row, 12, 0); \
        lv_obj_t *lbl = lv_label_create(row); \
        lv_label_set_text(lbl, label_text); \
        lv_obj_add_style(lbl, &style_about_label, 0); \
        lv_obj_t *val = lv_label_create(row); \
        lv_label_set_text(val, value_text); \
        lv_obj_add_style(val, &style_about_value, 0); \
    } while(0)

    CREATE_ABOUT_ROW(about_screen, 100, "Driver:", "S3-Touch-LCD-1.85B");
    CREATE_ABOUT_ROW(about_screen, 150, "fw version:", "V1.0.0");
    CREATE_ABOUT_ROW(about_screen, 200, "MAC:", get_mac_address());
    CREATE_ABOUT_ROW(about_screen, 250, "SDK:", "ESP_IDF_5.5.1");
}


static void init_about_lv_style(void)
{
    lv_style_init(&style_about_title);
    lv_style_set_text_font(&style_about_title, &lv_font_montserrat_22);
    lv_style_set_text_align(&style_about_title, LV_TEXT_ALIGN_CENTER);
    lv_style_set_text_color(&style_about_title, lv_color_hex(0x333333));
    lv_style_set_pad_bottom(&style_about_title, 30);

    lv_style_init(&style_about_label);
    lv_style_set_text_font(&style_about_label, &lv_font_montserrat_18);
    lv_style_set_text_color(&style_about_label, lv_color_hex(0x666666));

    lv_style_init(&style_about_value);
    lv_style_set_text_font(&style_about_value, &lv_font_montserrat_18);
    lv_style_set_text_color(&style_about_value, lv_color_hex(0x000000));
    lv_style_set_text_align(&style_about_value, LV_TEXT_ALIGN_RIGHT);

    lv_style_set_border_width(&style_about_value, 1);
    lv_style_set_border_side(&style_about_value, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_color(&style_about_value, lv_color_hex(0xE5E7EB));
}

static void back_btn_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        if(return_screen) {
            lv_scr_load(return_screen);
        }
    }
}

static char *get_mac_address(void)
{
    static char mac_str[18] = {0};
    uint8_t mac_addr[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, mac_addr);
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);
    return mac_str;
}