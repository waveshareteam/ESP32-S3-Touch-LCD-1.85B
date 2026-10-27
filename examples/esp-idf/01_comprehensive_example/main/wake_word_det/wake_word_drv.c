#include "wake_word_drv.h"

#include "bsp/esp-bsp.h"
#include "esp_task_wdt.h"
#include "bsp_board_extra.h"
#include "app_music/lvgl_music.h"

#include <stdio.h>
#include <stdlib.h>
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_process_sdkconfig.h"

#include "model_path.h"
#include "string.h"

int wakeup_flag = 0;
static const esp_afe_sr_iface_t *afe_handle = NULL;
static volatile int task_flag = 0;
static esp_codec_dev_handle_t record_dev = NULL;
srmodel_list_t *models = NULL;
int s_backlight = 0;
bool play_status = 0;

static TaskHandle_t feed_task_handle = NULL;
static TaskHandle_t det_task_handle = NULL;

#define ADC_I2S_CHANNEL 4

static esp_err_t bsp_get_feed_data(bool is_get_raw_channel, int16_t *buffer, int buffer_len)
{
    esp_err_t ret = ESP_OK;
    size_t bytes_read;
    int audio_chunksize = buffer_len / (sizeof(int16_t) * ADC_I2S_CHANNEL);

    ret = esp_codec_dev_read(record_dev, (void *)buffer, buffer_len);
    if (!is_get_raw_channel) {
        for (int i = 0; i < audio_chunksize; i++) {
            int16_t ref = buffer[4 * i + 0];
            buffer[3 * i + 0] = buffer[4 * i + 1];
            buffer[3 * i + 1] = buffer[4 * i + 3];
            buffer[3 * i + 2] = ref;
        }
    }

    return ret;
}
static int bsp_get_feed_channel(void)
{
    return ADC_I2S_CHANNEL;
}

static char* bsp_get_input_format(void)
{
    return "RMNM";
}

void feed_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int nch = afe_handle->get_feed_channel_num(afe_data);
    int feed_channel = bsp_get_feed_channel();
    assert(nch==feed_channel);
    int16_t *i2s_buff = malloc(audio_chunksize * sizeof(int16_t) * feed_channel);
    assert(i2s_buff);

    while (task_flag) {
        bsp_get_feed_data(true, i2s_buff, audio_chunksize * sizeof(int16_t) * feed_channel);

        afe_handle->feed(afe_data, i2s_buff);
        vTaskDelay(10);
    }
    if (i2s_buff) {
        free(i2s_buff);
        i2s_buff = NULL;
    }
    vTaskDelete(NULL);
}

void detect_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    printf("multinet:%s\n", mn_name);
    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
    model_iface_data_t *model_data = multinet->create(mn_name, 6000);
    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    esp_mn_commands_update_from_sdkconfig(multinet, model_data); // Add speech commands from sdkconfig
    assert(mu_chunksize == afe_chunksize);
    //print active speech commands
    multinet->print_active_speech_commands(model_data);

    printf("------------detect start------------\n");
    while (task_flag) {
        afe_fetch_result_t* res = afe_handle->fetch(afe_data); 
        if (!res || res->ret_value == ESP_FAIL) {
            printf("fetch error!\n");
            break;
        }

        if (res->wakeup_state == WAKENET_DETECTED) {
            printf("WAKEWORD DETECTED\n");
	        multinet->clean(model_data);
        }

        if (res->raw_data_channels == 1 && res->wakeup_state == WAKENET_DETECTED) {
            wakeup_flag = 1;
        } else if (res->raw_data_channels > 1 && res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
            // For a multi-channel AFE, it is necessary to wait for the channel to be verified.
            printf("AFE_FETCH_CHANNEL_VERIFIED, channel index: %d\n", res->trigger_channel_id);
            s_backlight = bsp_display_brightness_get();
            bsp_display_brightness_set(s_backlight-30);

            esp_asp_state_t status = Audio_Get_Current_State();
            if (status == ESP_ASP_STATE_RUNNING) {
                Audio_Pause_Play();
                bsp_display_lock(-1);
                ui_set_playing(false);
                bsp_display_unlock();

                play_status = true;
            }

            wakeup_flag = 1;
        }

        if (wakeup_flag == 1) {
            esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

            if (mn_state == ESP_MN_STATE_DETECTING) {
                continue;
            }

            if (mn_state == ESP_MN_STATE_DETECTED) {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                for (int i = 0; i < mn_result->num; i++) {
                    printf("TOP %d, command_id: %d, phrase_id: %d, string:%s prob: %f\n", 
                    i+1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->string, mn_result->prob[i]);
                }
                printf("\n-----------listening-----------\n");
            }

            if (mn_state == ESP_MN_STATE_TIMEOUT) {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                printf("timeout, string:%s\n", mn_result->string);
                afe_handle->enable_wakenet(afe_data);
                wakeup_flag = 0;
                printf("\n-----------awaits to be waken up-----------\n");

                esp_asp_state_t status = Audio_Get_Current_State();
                if (play_status == true) {
                    Audio_Resume_Play();
                    bsp_display_lock(-1);
                    ui_set_playing(true);
                    bsp_display_unlock();
                }

                bsp_display_brightness_set(s_backlight);
                play_status = false;

                continue;
            }
        }
    }
    if (model_data) {
        multinet->destroy(model_data);
        model_data = NULL;
    }
    printf("detect exit\n");
    vTaskDelete(NULL);
}

void wake_word_drv_init(void)
{

    record_dev =  bsp_audio_codec_microphone_init();

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 32,
        .channel = 2,
        .sample_rate = 16000,
    };
    esp_codec_dev_set_in_gain(record_dev, 30.0);
    esp_codec_dev_open(record_dev, &fs);

    models = esp_srmodel_init("model"); // partition label defined in partitions.csv
    afe_config_t *afe_config = afe_config_init(bsp_get_input_format(), models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    afe_handle = esp_afe_handle_from_config(afe_config);
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
    afe_config_free(afe_config);

    task_flag = 1;

    xTaskCreatePinnedToCore(&detect_Task, "detect", 8 * 1024, (void*)afe_data, 6, &feed_task_handle, 1);
    xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void*)afe_data, 5, &det_task_handle, 1);
}

void pause_wake_word_task()
{
    if (feed_task_handle) vTaskSuspend(feed_task_handle);
    if (det_task_handle) vTaskSuspend(det_task_handle);
}

void resume_wake_word_task()
{
    if (feed_task_handle) vTaskResume(feed_task_handle);
    if (det_task_handle) vTaskResume(det_task_handle);
}