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

void start_chipmunk_esp_example(qmi8658_dev_t *qmi8658_dev);
void stop_block_demo(void);


#ifdef __cplusplus
}
#endif
