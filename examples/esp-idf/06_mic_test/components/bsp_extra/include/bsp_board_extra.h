/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "bsp/esp-bsp.h"

#ifdef __cplusplus
extern "C" {
#endif
#define BSP_AUDIO_VOICE_CHANNELS 2
#define BSP_AUDIO_VOICE_RX_CHANNELS 4
#define BSP_AUDIO_VOICE_MIC_SLOT 1
#define BSP_AUDIO_VOICE_SECOND_MIC_SLOT 3
#define BSP_AUDIO_VOICE_REFERENCE_SLOT 0

typedef enum {
    BSP_EXTRA_AUDIO_OWNER_NONE,
    BSP_EXTRA_AUDIO_OWNER_MUSIC,
    BSP_EXTRA_AUDIO_OWNER_RECORDER,
    BSP_EXTRA_AUDIO_OWNER_XIAOZHI,
    BSP_EXTRA_AUDIO_OWNER_MAX,
} bsp_extra_audio_owner_t;

esp_err_t bsp_extra_codec_init(void);
bsp_extra_audio_owner_t bsp_extra_audio_session_get_owner(void);
const char *bsp_extra_audio_owner_name(bsp_extra_audio_owner_t owner);
esp_err_t bsp_extra_audio_session_acquire(bsp_extra_audio_owner_t owner);
esp_err_t bsp_extra_audio_session_release(bsp_extra_audio_owner_t owner);
/* Shared voice-session I/O: Xiaozhi captures four TDM samples (MIC1/MIC2/reference);
 * Mic Test uses two raw microphone samples, no AFE, and keeps PA off during capture.
 * Session ownership is required. Raw Mic Test playback accepts stereo PCM16. */
esp_err_t bsp_audio_voice_open(uint32_t rate, bool capture);
esp_err_t bsp_audio_voice_read(void *pcm, size_t bytes, size_t *read, uint32_t timeout_ms);
esp_err_t bsp_audio_voice_write(void *pcm, size_t bytes, size_t *written, uint32_t timeout_ms);
esp_err_t bsp_audio_voice_mute(bool mute);
esp_err_t bsp_audio_voice_close(void);

/* The calling worker owns the lease until its I/O and files have stopped. */
bool bsp_extra_audio_acquire(bsp_extra_audio_owner_t owner, uint32_t timeout_ms);
void bsp_extra_audio_release(void);
esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits, i2s_slot_mode_t channels);
esp_err_t bsp_extra_codec_dev_stop(void);
esp_err_t bsp_extra_codec_volume_set(int value, int *applied);
int bsp_extra_codec_volume_get(void);
esp_err_t bsp_extra_audio_drain(void);
/* Ordinary APPs select mono or stereo PCM; the speaker mixes stereo to mono. */
esp_err_t bsp_extra_i2s_read(void *pcm, size_t bytes, size_t *read, uint32_t timeout_ms);
esp_err_t bsp_extra_i2s_write(void *pcm, size_t bytes, size_t *written, uint32_t timeout_ms);
#ifdef __cplusplus
}
#endif
