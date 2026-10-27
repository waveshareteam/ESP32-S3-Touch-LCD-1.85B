#pragma once

#include "bsp/esp-bsp.h"
#include "bq27220.h"

#ifdef __cplusplus
extern "C" {
#endif

bq27220_handle_t bq27220_drv_init(void);
void init_bat_settings_screen(lv_obj_t *parent, lv_obj_t *ret_scr);

#ifdef __cplusplus
}
#endif
