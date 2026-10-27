#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"

static char *TAG = "app main";

static generic_file_list_t MP3_files;
uint16_t file_count;

void Search_Music(void) 
{        
    esp_err_t err = get_file_list_by_ext("/sdcard/music",".mp3",&MP3_files);
    if (err == ESP_OK) {
        for (int i = 0; i < MP3_files.count; i++) {
            printf("MP3:[%d]: %s\n", i, MP3_files.list[i]);
        }
    }
    file_count = MP3_files.count;                                           
}

void app_main()
{
    bsp_sdcard_mount();
    Search_Music();
    Audio_Play_Init();
    Volume_Adjustment(80);
    char uri[128];
    snprintf(uri, sizeof(uri), "file://%s", MP3_files.list[0] + strlen("/")); 
    Audio_Play_Music(uri);
}
