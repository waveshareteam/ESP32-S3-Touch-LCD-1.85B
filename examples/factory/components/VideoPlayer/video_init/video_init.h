#pragma once

#include "esp_log.h"
#include "bsp/esp-bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char **list;
    int count;
} generic_file_list_t;

esp_err_t get_file_list_by_ext(const char *base_path, const char *ext, generic_file_list_t *file_list);

void video_player_start(lv_display_t *disp,const char **file_list,int file_count);
void init_video_ui_screen(lv_obj_t *parent,generic_file_list_t *file_list);



esp_err_t video_player_play_path(const char *path);
void video_player_stop(void);
bool video_player_shutdown(void);

#ifdef __cplusplus
}
#endif
