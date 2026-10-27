#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char ssid[33];
    char password[65];
} wifi_credentials_t;

bool wifi_credentials_valid(const wifi_credentials_t *credentials);
bool wifi_credentials_parse(const char *json, size_t length, wifi_credentials_t *out);
