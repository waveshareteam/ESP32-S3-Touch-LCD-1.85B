#include "video_init.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdatomic.h>
#include "freertos/idf_additions.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"

#include "avi_player.h"       
#include "esp_jpeg_dec.h"     
#include "esp_lv_adapter.h"  
#include "lvgl.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#define VIDEO_W   360
#define VIDEO_H   360




/* 事件组和事件位 */
static EventGroupHandle_t s_video_evt = NULL;

#define VIDEO_EVT_PLAY      (1 << 0)  
#define VIDEO_EVT_STOP      (1 << 1)
#define VIDEO_EVT_REQUEST_STOP (1 << 2)
#define VIDEO_EVT_EXIT (1 << 3)
#define VIDEO_EVT_EXITED (1 << 4)
#define STRIP_ROWS 8
#define PREVIEW_SIZE 240
static atomic_bool s_playing;
static atomic_bool s_finished;
static atomic_bool s_cancelled;
static lv_timer_t *s_ui_timer;

static const char *TAG = "video_player_dummy";

typedef struct {
    avi_player_handle_t avi_handle;

    const char **avi_file_list;
    int avi_file_count;
    TaskHandle_t play_task_handle;

    char current_file[256];   // 当前要播放的文件

    lv_display_t      *disp;
    jpeg_dec_handle_t  jpeg;
    uint16_t          *framebuf;
    uint16_t *dma_strip;
    bool sd_lease; // Held until the AVI worker closes the playing file.
} video_player_t;

static video_player_t s_player;
static lv_obj_t * video_paly_bg = NULL;






esp_err_t video_player_play_path(const char *path)
{
    if (!path) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_player.play_task_handle || !s_video_evt) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 设置当前文件路径 */
    strncpy(s_player.current_file, path, sizeof(s_player.current_file) - 1);
    s_player.current_file[sizeof(s_player.current_file) - 1] = '\0';

    /* 发送 PLAY 事件 */
    xEventGroupSetBits(s_video_evt, VIDEO_EVT_PLAY);
    return ESP_OK;
}

void video_player_stop(void)
{
    atomic_store(&s_cancelled,true);
    if(s_video_evt) {
        xEventGroupClearBits(s_video_evt,VIDEO_EVT_PLAY);
        xEventGroupSetBits(s_video_evt,VIDEO_EVT_REQUEST_STOP);
    }
    // Only the GUI thread changes dummy-draw mode. Workers never take LVGL's lock.
    if(s_player.disp) esp_lv_adapter_set_dummy_draw(s_player.disp,false);
}


static esp_err_t video_dummy_init(video_player_t *vp)
{
    size_t buf_size = VIDEO_W * VIDEO_H * sizeof(uint16_t);

    // Decoder output stays in aligned PSRAM; only small strips occupy DMA RAM.
    vp->framebuf = (uint16_t *)heap_caps_aligned_calloc(16,1,buf_size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (!vp->framebuf) {
        ESP_LOGE(TAG, "no mem for framebuf");
        return ESP_ERR_NO_MEM;
    }

    vp->dma_strip=heap_caps_aligned_alloc(16,PREVIEW_SIZE*STRIP_ROWS*2,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
    if(!vp->dma_strip) { heap_caps_free(vp->framebuf); vp->framebuf=NULL; return ESP_ERR_NO_MEM; }
    jpeg_dec_config_t cfg = DEFAULT_JPEG_DEC_CONFIG();
    cfg.output_type = JPEG_PIXEL_FORMAT_RGB565_BE;

    jpeg_error_t jerr = jpeg_dec_open(&cfg, &vp->jpeg);
    if (jerr != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "jpeg_dec_open failed");
        heap_caps_free(vp->dma_strip); vp->dma_strip=NULL;
        heap_caps_free(vp->framebuf);
        vp->framebuf = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void video_dummy_deinit(video_player_t *vp)
{
    heap_caps_free(vp->dma_strip); vp->dma_strip=NULL;
    if (vp->jpeg) {
        jpeg_dec_close(vp->jpeg);
        vp->jpeg = NULL;
    }
    if (vp->framebuf) {
        heap_caps_free(vp->framebuf);
        vp->framebuf = NULL;
    }
}

static void video_dummy_draw_frame(video_player_t *vp, frame_data_t *data)
{
    if (!vp || !vp->disp || !vp->jpeg || !vp->framebuf) {
        return;
    }
    if (!data || !data->data || data->data_bytes == 0) {
        return;
    }

    jpeg_dec_io_t io = {
        .inbuf     = data->data,
        .inbuf_len = (int)data->data_bytes,
        .outbuf    = (uint8_t *)vp->framebuf,
    };

    jpeg_dec_header_info_t info;
    if (jpeg_dec_parse_header(vp->jpeg, &io, &info) != JPEG_ERR_OK) {
        ESP_LOGW(TAG, "jpeg_dec_parse_header failed");
        return;
    }

    int outbuf_len = 0;
    jpeg_dec_get_outbuf_len(vp->jpeg, &outbuf_len);
    if (outbuf_len > (int)(VIDEO_W * VIDEO_H * sizeof(uint16_t))) {
        ESP_LOGW(TAG, "outbuf too small");
        return;
    }

    if (jpeg_dec_process(vp->jpeg, &io) != JPEG_ERR_OK) {
        ESP_LOGW(TAG, "jpeg_dec_process failed");
        return;
    }
    if(!info.width || !info.height || info.width>VIDEO_W || info.height>VIDEO_H) return;
    unsigned w=PREVIEW_SIZE,h=info.height*w/info.width;
    if(h>PREVIEW_SIZE) { h=PREVIEW_SIZE; w=info.width*h/info.height; }
    // Even row widths keep each SPI transfer DMA aligned.
    w=(w<2?2:w)&~1U; h=h?h:1;
    const int xs=(VIDEO_W-w)/2,ys=(VIDEO_H-h)/2;
    for(unsigned y=0;y<h && !atomic_load(&s_cancelled);y+=STRIP_ROWS) {
        const unsigned rows=h-y<STRIP_ROWS?h-y:STRIP_ROWS;
        for(unsigned row=0;row<rows;++row)
            for(unsigned x=0;x<w;++x)
                vp->dma_strip[row*w+x]=vp->framebuf[((y+row)*info.height/h)*info.width+x*info.width/w];
        if(esp_lv_adapter_dummy_draw_blit(vp->disp,xs,ys+y,xs+w,ys+y+rows,vp->dma_strip,true)!=ESP_OK) break;
    }

}


static void video_cb(frame_data_t *data, void *arg)
{
    video_player_t *vp = (video_player_t *)arg;
    video_dummy_draw_frame(vp, data);
}

// static void audio_cb(frame_data_t *data, void *arg)
// {
//     (void)data;
//     (void)arg;
//     /* 这里按需做音频播放 */
// }

// static void audio_set_clock_cb(uint32_t rate, uint32_t bits_cfg, uint32_t ch, void *arg)
// {
//     (void)arg;
//     /* 在这里配置 I2S 时钟等 */
//     (void)rate;
//     (void)bits_cfg;
//     (void)ch;
// }

static void avi_play_done_cb(void *arg)
{
    (void)arg;
    atomic_store(&s_playing,false);
    atomic_store(&s_finished,true);
    xEventGroupSetBits(s_video_evt,VIDEO_EVT_STOP);
}

static void video_player_task(void *arg)
{
    video_player_t *vp = (video_player_t *)arg;

    avi_player_config_t cfg = {
        .buffer_size        = 512 * 1024,
        .video_cb           = video_cb,
        .audio_cb           = NULL,
        .audio_set_clock_cb = NULL,
        .avi_play_end_cb    = avi_play_done_cb,
        .priority           = 7,
        .coreID             = 0,
        .user_data          = vp,
        .stack_size         = 12 * 1024,
        .stack_in_psram     = true,
    };

    if (avi_player_init(cfg, &vp->avi_handle) != ESP_OK) {
        ESP_LOGE(TAG, "avi_player_init failed");
        xEventGroupSetBits(s_video_evt,VIDEO_EVT_EXITED);
        vTaskDeleteWithCaps(NULL);
        return;
    }

    if (video_dummy_init(vp) != ESP_OK) {
        avi_player_deinit(vp->avi_handle);
        vp->avi_handle = NULL;
        xEventGroupSetBits(s_video_evt,VIDEO_EVT_EXITED);
        vTaskDeleteWithCaps(NULL);
        return;
    }


    bool exiting=false;
    while (1)
    {
        /* 阻塞等待任意一个事件：PLAY / PAUSE / RESUME / STOP */
        EventBits_t bits = xEventGroupWaitBits(
            s_video_evt,
            VIDEO_EVT_PLAY | VIDEO_EVT_STOP | VIDEO_EVT_REQUEST_STOP | VIDEO_EVT_EXIT,
            pdTRUE,      
            pdFALSE,    
            portMAX_DELAY
        );

        if(bits & VIDEO_EVT_EXIT) exiting=true;
        if(bits & (VIDEO_EVT_REQUEST_STOP|VIDEO_EVT_EXIT)) {
            if(atomic_load(&s_playing)) avi_player_play_stop(vp->avi_handle);
        }
        if (bits & VIDEO_EVT_STOP) {
            if (vp->sd_lease) {
                bsp_sdcard_release();
                vp->sd_lease = false;
            }

        }

        if(exiting && !vp->sd_lease) break;
        if (!exiting && !atomic_load(&s_cancelled) && (bits & VIDEO_EVT_PLAY)) {
            if (vp->current_file[0] == '\0') {
                ESP_LOGE(TAG, "PLAY event but no file set");
                continue;
            }
            if (vp->sd_lease) continue;
            esp_err_t ret = bsp_sdcard_acquire();
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "SD card unavailable: %s", esp_err_to_name(ret));
                atomic_store(&s_finished,true);
                continue;
            }
            if(atomic_load(&s_cancelled)) {
                bsp_sdcard_release();
                atomic_store(&s_finished,true);
                continue;
            }
            vp->sd_lease = true;
            ESP_LOGI(TAG, "Play AVI: %s", vp->current_file);
            atomic_store(&s_playing,true);
            ret = avi_player_play_from_file(vp->avi_handle, vp->current_file);
            // Allow the AVI worker to consume its start event before a stop request.
            vTaskDelay(pdMS_TO_TICKS(20));
            if (ret != ESP_OK) {
                atomic_store(&s_playing,false);
                atomic_store(&s_finished,true);
                bsp_sdcard_release();
                vp->sd_lease = false;
                ESP_LOGW(TAG, "AVI playback failed: %s", esp_err_to_name(ret));
            }
        }
    }

    avi_player_deinit(vp->avi_handle);
    video_dummy_deinit(vp);

    xEventGroupSetBits(s_video_evt,VIDEO_EVT_EXITED);
    vTaskDeleteWithCaps(NULL);
}

void video_player_start(lv_display_t *disp,const char **file_list,int file_count)
{
    /* Reopening the APP reuses its player instead of leaking another worker. */
    if (s_player.play_task_handle) return;
    memset(&s_player, 0, sizeof(s_player));
    s_player.disp            = disp;
    s_player.avi_file_list   = file_list;
    s_player.avi_file_count  = file_count;
    s_player.current_file[0] = '\0';

    if (s_video_evt == NULL) {
        s_video_evt = xEventGroupCreate();
    }
    if (!s_video_evt) return;

    xEventGroupClearBits(s_video_evt,0xffffff);
    atomic_store(&s_playing,false); atomic_store(&s_finished,false); atomic_store(&s_cancelled,false);
    if (xTaskCreatePinnedToCoreWithCaps(
        video_player_task,
        "video_player_task",
        6 * 1024,
        &s_player,
        4,
        &s_player.play_task_handle,
        0, MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT
    ) != pdPASS) {
        s_player.play_task_handle = NULL;
        ESP_LOGE(TAG, "Video worker allocation failed");
    }
}







static lv_obj_t *list1;
static void ui_poll(lv_timer_t *timer) {
    (void)timer;
    if(atomic_exchange(&s_finished,false)) {
        esp_lv_adapter_set_dummy_draw(s_player.disp,false);
        if(video_paly_bg) lv_obj_add_flag(video_paly_bg,LV_OBJ_FLAG_HIDDEN);
    }
}
static void overlay_deleted(lv_event_t *e) {
    (void)e; video_paly_bg=NULL;
    if(s_ui_timer) { lv_timer_delete(s_ui_timer); s_ui_timer=NULL; }
}
static void select_video(lv_event_t *e) {
    if(atomic_load(&s_playing)) return;
    lv_obj_t *item=lv_event_get_target(e);
    if(!s_player.play_task_handle || (xEventGroupGetBits(s_video_evt)&VIDEO_EVT_EXITED)) return;
    atomic_store(&s_cancelled,false); atomic_store(&s_finished,false);
    lv_obj_remove_flag(video_paly_bg,LV_OBJ_FLAG_HIDDEN);
    esp_lv_adapter_refresh_now(s_player.disp);
    esp_lv_adapter_set_dummy_draw(s_player.disp,true);
    if(video_player_play_path(lv_list_get_button_text(list1,item))!=ESP_OK) atomic_store(&s_finished,true);
}
static void stop_video(lv_event_t *e) {
    (void)e; video_player_stop();
    if(video_paly_bg) lv_obj_add_flag(video_paly_bg,LV_OBJ_FLAG_HIDDEN);
}
bool video_player_shutdown(void) {
    video_player_stop();
    if(s_player.play_task_handle && s_video_evt) {
        xEventGroupSetBits(s_video_evt,VIDEO_EVT_EXIT);
        if(!(xEventGroupWaitBits(s_video_evt,VIDEO_EVT_EXITED,pdFALSE,pdTRUE,pdMS_TO_TICKS(5000))&VIDEO_EVT_EXITED))
            return false;
        s_player.play_task_handle=NULL;
    }
    if(s_ui_timer) { lv_timer_delete(s_ui_timer); s_ui_timer=NULL; }
    return true;
}
void init_video_ui_screen(lv_obj_t *parent,generic_file_list_t *files) {
    lv_obj_t *root=lv_obj_create(parent);
    lv_obj_remove_style_all(root); lv_obj_set_size(root,360,360);
    lv_obj_set_style_bg_color(root,lv_color_hex(0xf3f6fb),0);
    lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
    lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *title=lv_label_create(root);
    lv_label_set_text(title,"Videos"); lv_obj_set_pos(title,128,46);
    lv_obj_set_style_text_font(title,&lv_font_montserrat_20,0);
    lv_obj_set_style_text_color(title,lv_color_hex(0x1d2b42),0);
    list1=lv_list_create(root); lv_obj_set_pos(list1,56,88); lv_obj_set_size(list1,248,202);
    lv_obj_set_style_border_width(list1,0,0);
    lv_obj_set_scroll_dir(list1,LV_DIR_VER);
    for(int i=0;i<files->count;++i) {
        lv_obj_t *item=lv_list_add_button(list1,LV_SYMBOL_VIDEO,files->list[i]);
        lv_obj_set_style_text_font(item,&lv_font_montserrat_14,0);
        lv_obj_add_event_cb(item,select_video,LV_EVENT_CLICKED,NULL);
    }
    if(!files->count) {
        lv_obj_t *hint=lv_list_add_text(list1,"Insert an SD card with\nMJPEG AVI files in /video");
        lv_obj_set_style_text_font(hint,&lv_font_montserrat_14,0);
    }
    lv_obj_t *hint=lv_label_create(root); lv_label_set_text(hint,"Swipe up to return");
    lv_obj_set_style_text_font(hint,&lv_font_montserrat_12,0);
    lv_obj_set_style_text_color(hint,lv_color_hex(0x78859a),0);
    lv_obj_align(hint,LV_ALIGN_BOTTOM_MID,0,-30);
    video_paly_bg=lv_button_create(parent); lv_obj_remove_style_all(video_paly_bg);
    lv_obj_set_size(video_paly_bg,360,360);
    lv_obj_set_style_bg_color(video_paly_bg,lv_color_black(),0);
    lv_obj_set_style_bg_opa(video_paly_bg,LV_OPA_COVER,0);
    lv_obj_add_event_cb(video_paly_bg,overlay_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_add_event_cb(video_paly_bg,stop_video,LV_EVENT_CLICKED,NULL);
    lv_obj_add_flag(video_paly_bg,LV_OBJ_FLAG_HIDDEN);
    s_ui_timer=lv_timer_create(ui_poll,50,NULL);
}

esp_err_t get_file_list_by_ext(const char *dir_path, const char *extension, generic_file_list_t *out)
{
    if (!dir_path || !extension || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    out->list  = NULL;
    out->count = 0;

    DIR *dir = opendir(dir_path);
    if (!dir) {
        return ESP_FAIL;
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { 
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcasecmp(ext, extension) == 0) {
                count++;
            }
        }
    }

    if (count == 0) {
        closedir(dir);
        return ESP_ERR_NOT_FOUND;
    }

    char **list = (char **)malloc(sizeof(char *) * count);
    if (!list) {
        closedir(dir);
        return ESP_ERR_NO_MEM;
    }

    rewinddir(dir);
    int idx = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcasecmp(ext, extension) == 0 && idx < count) {

                size_t len = strlen(dir_path) + strlen(entry->d_name) + 2;
                char *full_path = (char *)malloc(len);
                if (!full_path) {
 
                    for (int i = 0; i < idx; i++) free(list[i]);
                    free(list);
                    closedir(dir);
                    return ESP_ERR_NO_MEM;
                }
                snprintf(full_path, len, "%s/%s", dir_path, entry->d_name);
                list[idx++] = full_path;
            }
        }
    }

    closedir(dir);
    out->list  = list;
    out->count = idx;
    return ESP_OK;
}
