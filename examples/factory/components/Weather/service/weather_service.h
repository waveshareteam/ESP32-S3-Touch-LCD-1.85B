#pragma once
#include "weather_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { WEATHER_WAIT_NETWORK, WEATHER_LOADING, WEATHER_READY, WEATHER_ERROR } weather_state_t;
typedef struct {
    weather_data_t data;
    weather_state_t state;
    bool cached;
    uint32_t generation;
    char message[64];
} weather_snapshot_t;

bool weather_service_start(void);
void weather_service_request_refresh(void);
void weather_service_get_snapshot(weather_snapshot_t *out);

#ifdef __cplusplus
}
#endif
