#pragma once

#include "bsp/esp-bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_style_t list;
    lv_style_t list_btn;
    lv_style_t list_text;
    lv_style_t list_btn_pressed;
    lv_style_t list_btn_hover;
    lv_style_t back_btn;
    lv_style_t back_btn_pressed;
} set_screen_styles_t;

set_screen_styles_t* get_set_screen_styles(void);

#ifdef __cplusplus
}
#endif
