/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once



#ifdef __cplusplus
extern "C" {
#endif


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_gmf_element.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_alc.h"
#include "esp_audio_simple_player.h"
#include "esp_audio_simple_player_advance.h"
#include "esp_codec_dev.h"
#include "esp_gmf_io.h"
#include "esp_gmf_io_embed_flash.h"
#include "esp_gmf_audio_dec.h"

#define Volume_MAX  100
#define VOLUME_DEFAULT  85

void Audio_Play_Init(void);
void Volume_Adjustment(uint8_t Vol);
uint8_t get_audio_volume(void);

esp_gmf_err_t Audio_Play_Music(const char* url);
esp_gmf_err_t Audio_Stop_Play(void);
esp_gmf_err_t Audio_Resume_Play(void);
esp_gmf_err_t Audio_Pause_Play(void);
esp_asp_state_t Audio_Get_Current_State(void);
void Audio_Play_Deinit(void);


#ifdef __cplusplus
}
#endif
