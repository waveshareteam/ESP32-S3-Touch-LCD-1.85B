// chipmunk_demo.h
#pragma once
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "chipmunk/chipmunk.h"
#include "chipmunk/chipmunk_structs.h"
#include "qmi8658.h"  


#ifdef __cplusplus
extern "C" {
#endif

void start_chipmunk_esp_example_1(qmi8658_dev_t *qmi8658);
void chipmunk_game_deinit(void);
void chipmunk_game_resume(void);
void chipmunk_game_pause(void);

#ifdef __cplusplus
}
#endif
