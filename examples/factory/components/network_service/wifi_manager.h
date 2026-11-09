#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include "esp_http_server.h"

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
/* A persistent content provider shares the home page, AP and HTTP server.
 * Register before wifi_manager_init(); callbacks must outlive every session. */
typedef esp_err_t (*wifi_local_portal_register_t)(httpd_handle_t server);
typedef void (*wifi_local_portal_stop_t)(void);
bool wifi_manager_register_local_portal(wifi_local_portal_register_t register_routes,
                                      wifi_local_portal_stop_t stop_editor);
bool wifi_manager_start_local_portal(wifi_local_portal_register_t register_routes);
void wifi_manager_stop_local_portal(void);
/* Browser ownership survives leaving either device APP. Wi-Fi configuration
 * is excluded while the image editor is open. Done releases the editor only;
 * Disconnect on the home page closes the hotspot. Browser leases idle out. */
bool wifi_manager_begin_web_transfer(void);
void wifi_manager_end_web_transfer(void);
bool wifi_manager_web_transfer_active(void);
void wifi_manager_touch_web_session(void);
void wifi_manager_get_snapshot(wifi_manager_snapshot_t *out);
bool wifi_manager_is_connected(void);
typedef enum {
    WIFI_CLOCK_UNSET,
    WIFI_CLOCK_RTC,
    WIFI_CLOCK_NETWORK,
} wifi_clock_source_t;
typedef struct {
    struct timeval instant;
    struct tm local;
    wifi_clock_source_t source;
} wifi_clock_snapshot_t;
/* Nonblocking Beijing-time reads, safe on the GUI task. The network worker
 * seeds the system clock from RTC once and writes SNTP corrections back to RTC.
 * Validity survives a disconnect; neither getter performs I2C or Flash access. */
bool wifi_manager_get_clock(wifi_clock_snapshot_t *out);
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
