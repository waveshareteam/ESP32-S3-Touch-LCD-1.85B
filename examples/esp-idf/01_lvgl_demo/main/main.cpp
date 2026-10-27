/* SPDX-License-Identifier: Apache-2.0 */
#include "bsp/esp-bsp.h"
#include "esp_check.h"

namespace {
unsigned clicks;
lv_obj_t *counter;
void clicked(lv_event_t *) { lv_label_set_text_fmt(counter, "Touch count: %u", ++clicks); }
void brightness(lv_event_t *event)
{
    auto *slider = static_cast<lv_obj_t *>(lv_event_get_target(event));
    bsp_display_brightness_set(lv_slider_get_value(slider));
}
}
extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(bsp_display_start() ? ESP_OK : ESP_FAIL);
    if (!bsp_display_lock(UINT32_MAX)) return;
    auto *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xf3f6fa), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0x203040), 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    auto *title = lv_label_create(screen);
    lv_label_set_text(title, "LVGL Touch Demo");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 56);
    counter = lv_label_create(screen);
    lv_label_set_text(counter, "Touch count: 0");
    lv_obj_align(counter, LV_ALIGN_CENTER, 0, -50);
    auto *button = lv_button_create(screen);
    lv_obj_set_size(button, 160, 48);
    lv_obj_center(button);
    auto *caption = lv_label_create(button);
    lv_label_set_text(caption, "Touch me"); lv_obj_center(caption);
    lv_obj_add_event_cb(button, clicked, LV_EVENT_CLICKED, nullptr);
    auto *slider = lv_slider_create(screen);
    lv_obj_set_size(slider, 200, 12);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 70);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, 100, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, brightness, LV_EVENT_VALUE_CHANGED, nullptr);
    auto *hint = lv_label_create(screen);
    lv_label_set_text(hint, "Backlight brightness");
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 103);
    bsp_display_unlock();
    ESP_ERROR_CHECK(bsp_display_show());
}
