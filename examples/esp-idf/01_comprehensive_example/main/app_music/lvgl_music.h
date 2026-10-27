#pragma once

#include "esp_log.h"
#include "bsp/esp-bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

void init_music_ui_screen(lv_obj_t *parent,generic_file_list_t *file_list);
void ui_set_playing(bool playing);
#ifdef __cplusplus
}
#endif
