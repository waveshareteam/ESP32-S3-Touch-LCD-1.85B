#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_DEFAULT_STA_SSID "weather-wifi"
#define WIFI_DEFAULT_STA_PASSWORD "12345678"
#define WIFI_PROVISION_PASSWORD "12345678"
#define WIFI_PROVISION_URL "http://192.168.4.1"

typedef struct {
    bool ready;
    bool connected;
    bool connecting;
    bool provisioning;
    bool credentials_saved;
    bool provisioning_success;
    char ssid[33];
    char ap_ssid[33];
    char message[80];
} wifi_manager_snapshot_t;

/* Start after BSP init. Prefer saved credentials; otherwise use weather-wifi. */
bool wifi_manager_init(void);
/* Explicit entry reserved for Settings. Never started by Weather or connection failure. */
bool wifi_manager_start_provisioning(void);
void wifi_manager_stop_provisioning(void);
void wifi_manager_get_snapshot(wifi_manager_snapshot_t *out);
bool wifi_manager_is_connected(void);
/* Nonblocking system-clock read in Beijing time; false until SNTP succeeds.
 * Remains valid after disconnect. Does not access the Sensor hardware RTC. */
bool wifi_manager_get_local_time(struct tm *out);

/* Read-only survey, serialized by the same worker that owns connection changes.
 * Results never select a network or change saved credentials. */
#define WIFI_SCAN_PAGE_SIZE 4
typedef struct {
    char ssid[33];
    char security[12];
    int8_t rssi;
    uint8_t channel;
} wifi_scan_record_t;
typedef struct {
    uint32_t revision;
    bool running;
    int32_t error;
    uint16_t count;
    uint16_t first;
    uint16_t page_count;
    wifi_scan_record_t records[WIFI_SCAN_PAGE_SIZE];
} wifi_scan_snapshot_t;
bool wifi_manager_request_scan(void);
/* count is the complete scan count; only the requested page is copied to out. */
void wifi_manager_get_scan_snapshot(wifi_scan_snapshot_t *out, uint16_t first);

#ifdef __cplusplus
}
#endif
