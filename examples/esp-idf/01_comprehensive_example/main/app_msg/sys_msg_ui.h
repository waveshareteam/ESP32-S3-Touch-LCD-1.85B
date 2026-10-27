#pragma once

#include "esp_log.h"
#include "bsp/esp-bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t msg_driver_init(void);
void msg_ui_screen_init(lv_obj_t *main_scr_tmp,lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
