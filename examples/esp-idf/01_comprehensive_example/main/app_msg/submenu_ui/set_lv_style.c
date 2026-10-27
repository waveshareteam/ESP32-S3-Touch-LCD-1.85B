#include "set_lv_style.h"

static set_screen_styles_t set_styles;
static bool s_style_init = false;
set_screen_styles_t* get_set_screen_styles(void)
{
    if(s_style_init)
    {
        return &set_styles;
    }

    lv_style_init(&set_styles.list);
    lv_style_init(&set_styles.list_btn);
    lv_style_init(&set_styles.list_text);
    lv_style_init(&set_styles.list_btn_pressed);
    lv_style_init(&set_styles.list_btn_hover);
    lv_style_init(&set_styles.back_btn);
    lv_style_init(&set_styles.back_btn_pressed);


    lv_style_set_pad_all(&set_styles.list, 8);          
    lv_style_set_pad_row(&set_styles.list, 4);          
    lv_style_set_border_width(&set_styles.list, 0);
    lv_style_set_radius(&set_styles.list, 12);          
    lv_style_set_bg_color(&set_styles.list, lv_color_hex(0xF8F9FA));  
    lv_style_set_bg_opa(&set_styles.list, LV_OPA_COVER);
    lv_style_set_shadow_width(&set_styles.list, 2);     
    lv_style_set_shadow_color(&set_styles.list, lv_color_hex(0x000000));
    lv_style_set_shadow_opa(&set_styles.list, LV_OPA_10);
    lv_style_set_shadow_offset_y(&set_styles.list, 2);

    lv_style_set_height(&set_styles.list_btn, 45);      
    lv_style_set_pad_left(&set_styles.list_btn, 10);    
    lv_style_set_pad_right(&set_styles.list_btn, 10);   
    lv_style_set_radius(&set_styles.list_btn, 8);       
    lv_style_set_bg_color(&set_styles.list_btn, lv_color_hex(0xFFFFFF));  
    lv_style_set_text_font(&set_styles.list_btn, &lv_font_montserrat_22);
    lv_style_set_text_color(&set_styles.list_btn, lv_color_hex(0x333333));  

    lv_style_set_border_width(&set_styles.list_btn, 1);
    lv_style_set_border_side(&set_styles.list_btn, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_color(&set_styles.list_btn, lv_color_hex(0xE5E7EB));  

    lv_style_set_text_font(&set_styles.list_text, &lv_font_montserrat_22);
    lv_style_set_text_color(&set_styles.list_text, lv_color_hex(0x6C757D));  
    lv_style_set_bg_color(&set_styles.list_text, lv_color_hex(0xF8F9FA));    
    lv_style_set_bg_opa(&set_styles.list_text, LV_OPA_COVER);
    lv_style_set_pad_all(&set_styles.list_text, 10);
    lv_style_set_pad_bottom(&set_styles.list_text, 5);

    lv_style_set_bg_color(&set_styles.list_btn_pressed, lv_color_hex(0xEFF6FF));  
    lv_style_set_text_color(&set_styles.list_btn_pressed, lv_color_hex(0x0D6EFD));  
    lv_style_set_border_color(&set_styles.list_btn_pressed, lv_color_hex(0xE5E7EB));  

    lv_style_set_bg_color(&set_styles.list_btn_hover, lv_color_hex(0xF9FBFF));  
    lv_style_set_text_color(&set_styles.list_btn_hover, lv_color_hex(0x333333));

    lv_style_set_size(&set_styles.back_btn, 65, 40);
    lv_style_set_radius(&set_styles.back_btn, 20);
    lv_style_set_bg_color(&set_styles.back_btn, lv_color_hex(0xFFFFFF));
    lv_style_set_shadow_width(&set_styles.back_btn, 2);
    lv_style_set_text_font(&set_styles.back_btn, &lv_font_montserrat_18);
    lv_style_set_text_color(&set_styles.back_btn, lv_color_hex(0x333333));

    lv_style_set_bg_color(&set_styles.back_btn_pressed, lv_color_hex(0xEFF6FF));
    lv_style_set_text_color(&set_styles.back_btn_pressed, lv_color_hex(0x0D6EFD));
    return &set_styles;
}

