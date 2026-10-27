#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    void *context;
    int (*read)(void *context,char *buffer,size_t size);
    bool (*complete)(void *context);
    bool (*expired)(void *context);
} weather_body_reader_t;
typedef enum { WEATHER_BODY_OK, WEATHER_BODY_IO, WEATHER_BODY_INCOMPLETE, WEATHER_BODY_TOO_LARGE, WEATHER_BODY_TIMEOUT } weather_body_result_t;

weather_body_result_t weather_body_read(const weather_body_reader_t *reader,char *buffer,size_t capacity,size_t *length);
