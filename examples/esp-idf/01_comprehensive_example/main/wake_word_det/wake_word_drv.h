#pragma once

#include "esp_log.h"
#include "bsp/esp-bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

void wake_word_drv_init(void);
void pause_wake_word_task();
void resume_wake_word_task();
#ifdef __cplusplus
}
#endif
