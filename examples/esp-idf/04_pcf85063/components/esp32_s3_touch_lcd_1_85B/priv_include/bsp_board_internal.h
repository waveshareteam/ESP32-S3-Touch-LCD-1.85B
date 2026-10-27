#pragma once
#include "bsp/esp-bsp.h"

extern bsp_init_status_t bsp_board_status;
esp_err_t bsp_board_init_locks(void);
bool bsp_board_i2c_in_use(void);
bool bsp_board_audio_in_use(void);
