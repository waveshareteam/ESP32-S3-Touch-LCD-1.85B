#pragma once

#include "bsp/esp-bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

void wifi_init_sta(void);
void init_wifi_service_screen(lv_obj_t *parent, lv_obj_t *ret_scr);

#ifdef __cplusplus
}
#endif
