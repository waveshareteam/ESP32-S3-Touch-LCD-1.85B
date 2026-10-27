#pragma once
#include "bsp/esp-bsp.h"
#include "qmi8658.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_err_t i2c, display, touch;
    esp_err_t button, audio, rtc, imu, sdcard, wifi, battery;
} bsp_init_status_t;

/* Call once from app_main before starting APPs. Required display errors are
 * returned; optional peripheral failures are logged and stored in status. */
esp_err_t bsp_init(void);
const bsp_init_status_t *bsp_get_init_status(void);

lv_display_t *bsp_display_get(void);
esp_lcd_touch_handle_t bsp_touch_get_handle(void);
esp_err_t bsp_user_button_init(void);
uint32_t bsp_user_button_get_press_count(void);

typedef enum {
    BSP_BUTTON_ID_BOOT,
} bsp_button_id_t;
/* Normalized physical state: BOOT GPIO0 is active-low. */
esp_err_t bsp_button_get_pressed(bsp_button_id_t button, bool *pressed);

typedef struct {
    uint16_t year;
    uint8_t month, day, hour, minute, second, weekday;
} bsp_sensor_datetime_t;

typedef struct {
    float accel_x, accel_y, accel_z; /* g */
    float gyro_x, gyro_y, gyro_z;    /* degrees/second */
    float temperature_c;
} bsp_imu_data_t;
esp_err_t bsp_rtc_init(void);
esp_err_t bsp_rtc_get_datetime(bsp_sensor_datetime_t *datetime);
esp_err_t bsp_rtc_set_datetime(const bsp_sensor_datetime_t *datetime);
esp_err_t bsp_imu_init(void);
esp_err_t bsp_imu_get_data(bsp_imu_data_t *data);
/* The original gravity game uses the same BSP-owned QMI8658 handle.
 * Its acceleration unit remains m/s²; do not reconfigure or delete the handle. */
qmi8658_dev_t *bsp_imu_get_handle(void);

typedef struct {
    bool present, initialized;
    uint16_t voltage_mv, percent, remaining_mah, full_mah, design_mah, cycles;
    int16_t current_ma, average_current_ma; /* Positive: charge; negative: discharge. */
    float temperature_c; /* Gauge-reported battery temperature, not ambient. */
    uint16_t battery_status, operation_status;
} bsp_battery_data_t;
/* Verify/apply the factory 3.7 V / 500 mAh cell profile once before sampling.
 * Matching parameters and learned full-charge capacity are preserved. */
esp_err_t bsp_battery_init(void);
/* On failure, *data is unchanged. Check present/initialized before using values. */
esp_err_t bsp_battery_get_data(bsp_battery_data_t *data);

typedef struct {
    bool mounted;
    uint64_t capacity_bytes, total_bytes, free_bytes;
} bsp_sdcard_info_t;
esp_err_t bsp_sdcard_mount(void);
/* Active APPs acquire/release a lease so Settings cannot unmount an in-use card. */
esp_err_t bsp_sdcard_acquire(void);
void bsp_sdcard_release(void);
esp_err_t bsp_sdcard_unmount(void);
esp_err_t bsp_sdcard_get_info(bsp_sdcard_info_t *info);

/* Driver/netif initialization only. Credentials/scan/provisioning belong to APPs. */
esp_err_t bsp_wifi_init(void);
/* Start the initialized radio for scanning/provisioning. Does not connect. */
esp_err_t bsp_wifi_start(void);

#ifdef __cplusplus
}
#endif
