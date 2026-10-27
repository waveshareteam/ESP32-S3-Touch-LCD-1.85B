#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    WEATHER_UNKNOWN, WEATHER_SUNNY, WEATHER_CLOUDY, WEATHER_OVERCAST,
    WEATHER_RAIN, WEATHER_THUNDER, WEATHER_SNOW, WEATHER_FOG, WEATHER_ICON_COUNT
} weather_condition_t;

typedef struct {
    char city[48];
    char date[16];
    char weather[32];
    char temp[16];
    char humi[16];
    char wind_speed[16];
    char wind_dir[8];
    weather_condition_t condition;
} weather_data_t;

typedef enum { WEATHER_PARSE_OK, WEATHER_PARSE_INVALID, WEATHER_PARSE_API_ERROR } weather_parse_result_t;

weather_parse_result_t weather_parse_json(const char *json, size_t length, const char *city, weather_data_t *out);
bool weather_build_url(char *out, size_t capacity, const char *id, const char *key,
                       const char *province, const char *city);
