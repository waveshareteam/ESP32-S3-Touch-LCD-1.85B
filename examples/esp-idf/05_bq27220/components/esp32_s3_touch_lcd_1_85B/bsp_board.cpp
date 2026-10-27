#include "bsp/esp-bsp.h"

#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "bsp_board_extra.h"
#include "bsp_board_internal.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "pcf85063a.h"

static const char *TAG = "bsp_1_85B";
static SemaphoreHandle_t sensor_mutex;
static pcf85063a_dev_t rtc;
static qmi8658_dev_t imu;
static i2c_master_dev_handle_t battery;
static bool battery_profile_ready;
static bool rtc_ready, imu_ready, wifi_ready;
static button_handle_t boot_button;
static std::atomic<uint32_t> button_presses{0};
bsp_init_status_t bsp_board_status = {
    ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_STATE,
    ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_STATE,
    ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_STATE,
    ESP_ERR_INVALID_STATE
};

class SensorLock {
public:
    SensorLock() { xSemaphoreTakeRecursive(sensor_mutex, portMAX_DELAY); }
    ~SensorLock() { xSemaphoreGiveRecursive(sensor_mutex); }
};

esp_err_t bsp_board_init_locks(void)
{
    if (!sensor_mutex) sensor_mutex = xSemaphoreCreateRecursiveMutex();
    if (!sensor_mutex) return ESP_ERR_NO_MEM;
    return ESP_OK;
}

bool bsp_board_i2c_in_use(void)
{
    return rtc.dev_handle || imu.dev_handle || battery || bsp_board_audio_in_use();
}

const bsp_init_status_t *bsp_get_init_status(void) { return &bsp_board_status; }

static void button_pressed(void *, void *) { button_presses.fetch_add(1); }
esp_err_t bsp_user_button_init(void)
{
    if (boot_button) return ESP_OK;
    button_config_t cfg = {};
    button_gpio_config_t gpio = {};
    gpio.gpio_num = BSP_BUTTON_BOOT;
    gpio.active_level = 0;
    ESP_RETURN_ON_ERROR(iot_button_new_gpio_device(&cfg, &gpio, &boot_button), TAG, "BOOT button");
    esp_err_t ret = iot_button_register_cb(boot_button, BUTTON_PRESS_DOWN, nullptr, button_pressed, nullptr);
    if (ret != ESP_OK) { iot_button_delete(boot_button); boot_button = nullptr; }
    return ret;
}

uint32_t bsp_user_button_get_press_count(void) { return button_presses.load(); }

esp_err_t bsp_button_get_pressed(bsp_button_id_t button, bool *pressed)
{
    if (!pressed) return ESP_ERR_INVALID_ARG;
    switch (button) {
    case BSP_BUTTON_ID_BOOT:
        if (!boot_button) return ESP_ERR_INVALID_STATE;
        *pressed = gpio_get_level(BSP_BUTTON_BOOT) == 0;
        return ESP_OK;
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

static bool valid_datetime(const bsp_sensor_datetime_t &d)
{
    if (d.year < 2000 || d.year > 2099 || d.month < 1 || d.month > 12 ||
        d.day < 1 || d.hour > 23 || d.minute > 59 || d.second > 59) return false;
    const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return d.day <= days[d.month - 1] + (d.month == 2 && d.year % 4 == 0);
}

static int64_t rtc_calendar_seconds(const bsp_sensor_datetime_t &d)
{
    int64_t days = 0;
    for (unsigned year = 2000; year < d.year; ++year) days += year % 4 == 0 ? 366 : 365;
    const uint8_t month_days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    for (unsigned month = 1; month < d.month; ++month)
        days += month_days[month - 1] + (month == 2 && d.year % 4 == 0);
    return ((days + d.day - 1) * 24 + d.hour) * 3600 + d.minute * 60 + d.second;
}

enum class RtcReadState { Unread, Ready, NotInitialized, TransferFailed, OscillatorStopped, InvalidCalendar };
static RtcReadState rtc_read_state = RtcReadState::Unread;

// Called with SensorLock held. Report a changed failure, rather than logging every poll.
static esp_err_t rtc_read_failed(RtcReadState state, esp_err_t error, const char *reason, const uint8_t *raw = nullptr)
{
    if (rtc_read_state != state) {
        ESP_LOGW(TAG, "RTC read rejected: %s (%s)", reason, esp_err_to_name(error));
        if (raw) ESP_LOGW(TAG, "RTC registers 0x04..0x0A: %02X %02X %02X %02X %02X %02X %02X",
            raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6]);
    }
    rtc_read_state = state;
    return error;
}

esp_err_t bsp_rtc_get_datetime(bsp_sensor_datetime_t *datetime)
{
    if (!datetime) return ESP_ERR_INVALID_ARG;
    SensorLock lock;
    if (!rtc_ready) return rtc_read_failed(RtcReadState::NotInitialized, ESP_ERR_INVALID_STATE, "device not initialized");
    uint8_t raw[7] = {};
    const uint8_t reg = PCF85063A_RTC_SECOND_ADDR;
    esp_err_t ret = i2c_master_transmit_receive(rtc.dev_handle, &reg, 1, raw, sizeof(raw), 100);
    if (ret != ESP_OK) return rtc_read_failed(RtcReadState::TransferFailed, ret, "I2C transaction failed");
    if (raw[0] & 0x80) return rtc_read_failed(RtcReadState::OscillatorStopped,
        ESP_ERR_INVALID_STATE, "oscillator-stop flag set; calendar is not reliable", raw);
    raw[0] &= 0x7f; raw[1] &= 0x7f; raw[2] &= 0x3f;
    raw[3] &= 0x3f; raw[4] &= 7; raw[5] &= 0x1f;
    for (uint8_t byte : raw) {
        if ((byte & 15) > 9 || (byte >> 4) > 9) return rtc_read_failed(
            RtcReadState::InvalidCalendar, ESP_ERR_INVALID_RESPONSE, "invalid BCD calendar", raw);
    }
    auto dec = [](uint8_t v) -> uint8_t { return (v >> 4) * 10 + (v & 15); };
    bsp_sensor_datetime_t result = { (uint16_t)(2000 + dec(raw[6])), dec(raw[5]), dec(raw[3]),
                                    dec(raw[2]), dec(raw[1]), dec(raw[0]), dec(raw[4]) };
    if (!valid_datetime(result) || result.weekday > 6) return rtc_read_failed(
        RtcReadState::InvalidCalendar, ESP_ERR_INVALID_RESPONSE, "calendar fields out of range", raw);
    if (rtc_read_state != RtcReadState::Ready) ESP_LOGI(TAG, "RTC read verified: %04u-%02u-%02u %02u:%02u:%02u",
        result.year, result.month, result.day, result.hour, result.minute, result.second);
    rtc_read_state = RtcReadState::Ready;
    *datetime = result;
    return ESP_OK;
}

esp_err_t bsp_rtc_set_datetime(const bsp_sensor_datetime_t *datetime)
{
    if (!datetime || !valid_datetime(*datetime) || datetime->weekday > 6) return ESP_ERR_INVALID_ARG;
    SensorLock lock;
    if (!rtc_ready) return ESP_ERR_INVALID_STATE;
    auto bcd = [](uint8_t value) -> uint8_t { return ((value / 10) << 4) | (value % 10); };
    // PCF85063 stores the two-digit year; keep the same 2000..2099 epoch as S3-3.5.
    const uint8_t raw[] = {PCF85063A_RTC_SECOND_ADDR, bcd(datetime->second), bcd(datetime->minute),
        bcd(datetime->hour), bcd(datetime->day), bcd(datetime->weekday),
        bcd(datetime->month), bcd(datetime->year - 2000)};
    esp_err_t ret = i2c_master_transmit(rtc.dev_handle, raw, sizeof(raw), 100);
    if (ret != ESP_OK) return rtc_read_failed(RtcReadState::TransferFailed, ret, "I2C calendar write failed");
    // A successful I2C write alone does not prove the oscillator/calendar is valid.
    bsp_sensor_datetime_t verified = {};
    ret = bsp_rtc_get_datetime(&verified);
    if (ret != ESP_OK) return ret;
    const int64_t elapsed = rtc_calendar_seconds(verified) - rtc_calendar_seconds(*datetime);
    // Allow the second to tick, including across a day/month/year boundary.
    if (elapsed < 0 || elapsed > 1 || (elapsed == 0 && verified.weekday != datetime->weekday)) {
        ESP_LOGW(TAG, "RTC calendar readback differs from the requested time");
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

esp_err_t bsp_rtc_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C");
    SensorLock lock;
    esp_err_t ret = ESP_OK;
    if (!rtc_ready) ret = pcf85063a_init(&rtc, bsp_i2c_get_handle(), PCF85063A_ADDRESS);
    if (ret == ESP_OK) {
        rtc_ready = true;
        bsp_sensor_datetime_t date = {};
        ret = bsp_rtc_get_datetime(&date);
        // The I2C driver also returns ESP_ERR_INVALID_STATE for failed transfers.
        // Only successful register reads that prove OS/invalid calendar may
        // trigger a default time write; a transport error must preserve time.
        if (ret != ESP_OK && (rtc_read_state == RtcReadState::OscillatorStopped ||
                              rtc_read_state == RtcReadState::InvalidCalendar)) {
            const bsp_sensor_datetime_t fallback = {2025, 1, 1, 12, 0, 0, 3};
            // After first power-up OS may remain set until the crystal is stable.
            // Retry writing/reading the calendar for at most two seconds; never
            // reset a running RTC with an already valid calendar.
            for (unsigned attempt = 0; attempt <= 20; ++attempt) {
                ret = bsp_rtc_set_datetime(&fallback);
                if (ret == ESP_OK || rtc_read_state != RtcReadState::OscillatorStopped || attempt == 20) break;
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            if (ret == ESP_OK) ESP_LOGW(TAG, "RTC calendar initialized and verified at 2025-01-01 12:00:00");
        }
    }
    if (ret != ESP_OK) {
        rtc_ready = false;
        if (rtc.dev_handle) i2c_master_bus_rm_device(rtc.dev_handle);
        rtc = {};
    }
    return ret;
}

esp_err_t bsp_imu_init(void)
{
    if (imu_ready) return ESP_OK;
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C");
    SensorLock lock;
    auto configure = [&]() -> esp_err_t {
        ESP_RETURN_ON_ERROR(qmi8658_init(&imu, bsp_i2c_get_handle(), QMI8658_ADDRESS_HIGH), TAG, "QMI8658");
        // Preserve the original gravity game's scale, sampling rate and SI units.
        ESP_RETURN_ON_ERROR(qmi8658_set_accel_range(&imu, QMI8658_ACCEL_RANGE_8G), TAG, "IMU range");
        ESP_RETURN_ON_ERROR(qmi8658_set_accel_odr(&imu, QMI8658_ACCEL_ODR_500HZ), TAG, "IMU rate");
        qmi8658_set_accel_unit_mps2(&imu, true);
        return qmi8658_write_register(&imu, QMI8658_CTRL5, 0x03);
    };
    const esp_err_t ret = configure();
    if (ret == ESP_OK) imu_ready = true;
    else {
        if (imu.dev_handle) i2c_master_bus_rm_device(imu.dev_handle);
        imu = {};
    }
    return ret;
}

esp_err_t bsp_imu_get_data(bsp_imu_data_t *data)
{
    if (!data) return ESP_ERR_INVALID_ARG;
    if (!imu_ready) return ESP_ERR_INVALID_STATE;
    SensorLock lock;
    qmi8658_data_t sample = {};
    ESP_RETURN_ON_ERROR(qmi8658_read_sensor_data(&imu, &sample), TAG, "IMU read");
    // The APP-facing values use g; the original game keeps its m/s² handle.
    *data = {sample.accelX / ONE_G, sample.accelY / ONE_G, sample.accelZ / ONE_G,
             sample.gyroX, sample.gyroY, sample.gyroZ, sample.temperature};
    return ESP_OK;
}

qmi8658_dev_t *bsp_imu_get_handle(void)
{
    return bsp_imu_init() == ESP_OK ? &imu : nullptr;
}

// Production 3.7 V / 500 mAh cell: preserve the original factory CEDV profile.
// Data-memory values are big-endian; standard command words are little-endian.
// Full-charge capacity is seeded only when migrating a mismatched profile.
// It is deliberately excluded from the comparison to preserve learned capacity.
static constexpr uint16_t BATTERY_DESIGN_MAH = 500;
struct BatteryParameter { uint16_t address, value; uint8_t size; };
static constexpr BatteryParameter battery_profile[] = {
    {0x929b, 0x0d11, 2}, // CCT, SC, FCC_LIM, FC_FOR_VDQ, IGNORE_SD
    {0x929f, BATTERY_DESIGN_MAH, 2},
    {0x926d, 0, 2}, {0x926b, 200, 2}, {0x9268, 20, 1},
    {0x92b4, 3490, 2}, {0x92b7, 3511, 2}, {0x92ba, 3535, 2},
    {0x92a7, 3670, 2}, {0x92a9, 115, 2}, {0x92ab, 968, 2},
    {0x92ad, 4547, 2}, {0x92af, 4764, 2}, {0x92b1, 11, 1}, {0x92b2, 0, 1},
    {0x92bd, 4147, 2}, {0x92bf, 4002, 2}, {0x92c1, 3969, 2},
    {0x92c3, 3938, 2}, {0x92c5, 3880, 2}, {0x92c7, 3824, 2},
    {0x92c9, 3794, 2}, {0x92cb, 3753, 2}, {0x92cd, 3677, 2},
    {0x92cf, 3574, 2}, {0x92d1, 3490, 2},
};

static esp_err_t battery_read_word(uint8_t reg, uint16_t &word)
{
    uint8_t raw[2];
    esp_err_t ret = i2c_master_transmit_receive(battery, &reg, 1, raw, sizeof(raw), 100);
    if (ret == ESP_OK) word = uint16_t(raw[0]) | (uint16_t(raw[1]) << 8);
    return ret;
}

static esp_err_t battery_control(uint16_t command)
{
    const uint8_t raw[] = {0x00, uint8_t(command), uint8_t(command >> 8)};
    return i2c_master_transmit(battery, raw, sizeof(raw), 100);
}

static esp_err_t battery_wait_status(uint16_t mask, uint16_t expected)
{
    for (unsigned attempt = 0; attempt < 60; ++attempt) {
        uint16_t status = 0;
        esp_err_t ret = battery_read_word(0x3a, status);
        if (ret != ESP_OK) return ret;
        if ((status & mask) == expected) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return ESP_ERR_TIMEOUT;
}

static esp_err_t battery_read_parameter(const BatteryParameter &parameter, uint16_t &value)
{
    const uint8_t select[] = {0x3e, uint8_t(parameter.address), uint8_t(parameter.address >> 8)};
    esp_err_t ret = i2c_master_transmit(battery, select, sizeof(select), 100);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));
    // Read the echoed address too, to reject a stale MAC response.
    const uint8_t reg = 0x3e;
    uint8_t raw[4] = {};
    ret = i2c_master_transmit_receive(battery, &reg, 1, raw, 2 + parameter.size, 100);
    if (ret != ESP_OK) return ret;
    if (raw[0] != select[1] || raw[1] != select[2]) return ESP_ERR_INVALID_RESPONSE;
    value = parameter.size == 1 ? raw[2] : (uint16_t(raw[2]) << 8) | raw[3];
    return ESP_OK;
}

static esp_err_t battery_write_parameter(const BatteryParameter &parameter)
{
    uint8_t raw[] = {0x3e, uint8_t(parameter.address), uint8_t(parameter.address >> 8),
        uint8_t(parameter.size == 1 ? parameter.value : parameter.value >> 8), uint8_t(parameter.value)};
    esp_err_t ret = i2c_master_transmit(battery, raw, 3 + parameter.size, 100);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));
    uint8_t sum = 0;
    for (unsigned i = 1; i < 3 + parameter.size; ++i) sum += raw[i];
    const uint8_t commit[] = {0x60, uint8_t(0xff - sum), uint8_t(4 + parameter.size)};
    ret = i2c_master_transmit(battery, commit, sizeof(commit), 100);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));
    uint16_t verified = 0;
    ret = battery_read_parameter(parameter, verified);
    if (ret != ESP_OK) return ret;
    return verified == parameter.value ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

static esp_err_t battery_apply_profile(bool &updated)
{
    bool matches = true;
    for (const auto &parameter : battery_profile) {
        uint16_t value = 0;
        esp_err_t ret = battery_read_parameter(parameter, value);
        if (ret != ESP_OK) return ret;
        if (value != parameter.value) matches = false;
    }
    uint16_t design = 0;
    esp_err_t ret = battery_read_word(0x3c, design);
    if (ret != ESP_OK) return ret;
    if (matches && design == BATTERY_DESIGN_MAH) return ESP_OK;

    ESP_LOGI(TAG, "Applying factory 500 mAh battery profile (previous design: %u mAh)", design);
    uint16_t status = 0;
    ret = battery_read_word(0x3a, status);
    if (ret != ESP_OK) return ret;
    if (!(status & 0x0400)) {
        ret = battery_control(0x0090); // ENTER_CFG_UPDATE
        if (ret != ESP_OK) return ret;
    }
    ret = battery_wait_status(0x0400, 0x0400);
    if (ret != ESP_OK) return ret;
    // Initial FCC is 500 mAh; later learned values are not reset on each boot.
    ret = battery_write_parameter({0x929d, BATTERY_DESIGN_MAH, 2});
    if (ret != ESP_OK) return ret;
    for (const auto &parameter : battery_profile) {
        ret = battery_write_parameter(parameter);
        if (ret != ESP_OK) return ret;
    }
    updated = true;
    return ESP_OK;
}

static esp_err_t battery_prepare_profile(void)
{
    // All MAC operations are serialized with the BSP sensor lock.
    esp_err_t ret = battery_control(0x0001); // DEVICE_NUMBER
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(15));
    uint16_t device = 0, status = 0;
    ret = battery_read_word(0x40, device);
    if (ret != ESP_OK) return ret;
    if (device != 0x0220) return ESP_ERR_INVALID_RESPONSE;
    ret = battery_read_word(0x3a, status);
    if (ret != ESP_OK) return ret;
    if ((status & 0x0006) == 0x0006) {
        ret = battery_control(0x0414);
        if (ret == ESP_OK) { vTaskDelay(pdMS_TO_TICKS(10)); ret = battery_control(0x3672); }
        if (ret == ESP_OK) ret = battery_wait_status(0x0006, 0x0004);
    } else if ((status & 0x0006) == 0) ret = ESP_ERR_INVALID_RESPONSE;

    bool updated = false;
    if (ret == ESP_OK) ret = battery_apply_profile(updated);
    // Always attempt to leave config mode and reseal, including after an error.
    esp_err_t cleanup = battery_read_word(0x3a, status);
    if (cleanup == ESP_OK && (status & 0x0400)) {
        cleanup = battery_control(0x0091); // EXIT_CFG_UPDATE_REINIT
        if (cleanup == ESP_OK) cleanup = battery_wait_status(0x0420, 0x0020);
    }
    if (ret == ESP_OK) ret = cleanup;
    cleanup = battery_control(0x0030); // SEALED
    if (cleanup == ESP_OK) cleanup = battery_wait_status(0x0006, 0x0006);
    if (ret == ESP_OK) ret = cleanup;
    if (ret != ESP_OK) return ret;

    uint16_t design = 0, full = 0;
    ret = battery_read_word(0x3c, design);
    if (ret == ESP_OK) ret = battery_read_word(0x12, full);
    if (ret != ESP_OK) return ret;
    if (design != BATTERY_DESIGN_MAH || full == 0) return ESP_ERR_INVALID_RESPONSE;
    ESP_LOGI(TAG, "500 mAh battery profile %s; design=%u mAh, full=%u mAh",
             updated ? "applied and verified" : "verified, no parameter writes", design, full);
    return ESP_OK;
}

esp_err_t bsp_battery_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C");
    SensorLock lock;
    if (battery && battery_profile_ready) return ESP_OK;
    esp_err_t ret = ESP_OK;
    if (!battery) {
        const auto bus = bsp_i2c_get_handle();
        ret = i2c_master_probe(bus, 0x55, 100);
        if (ret == ESP_OK) {
            i2c_device_config_t cfg = {};
            cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
            cfg.device_address = 0x55;
            cfg.scl_speed_hz = 400000;
            ret = i2c_master_bus_add_device(bus, &cfg, &battery);
        }
    }
    if (ret == ESP_OK) ret = battery_prepare_profile();
    battery_profile_ready = ret == ESP_OK;
    bsp_board_status.battery = ret;
    return ret;
}

esp_err_t bsp_battery_get_data(bsp_battery_data_t *data)
{
    if (!data) return ESP_ERR_INVALID_ARG;
    if (!sensor_mutex) return ESP_ERR_INVALID_STATE;
    SensorLock lock;
    if (!battery) return ESP_ERR_INVALID_STATE;
    if (!battery_profile_ready) return ESP_ERR_NOT_FINISHED;
    bsp_battery_data_t sample = {};
    esp_err_t ret = battery_read_word(0x0a, sample.battery_status);
    if (ret != ESP_OK) return ret;
    ret = battery_read_word(0x3a, sample.operation_status);
    if (ret != ESP_OK) return ret;
    sample.present = (sample.battery_status & (1u << 3)) != 0;
    sample.initialized = (sample.operation_status & (1u << 5)) &&
                         !(sample.operation_status & (1u << 10));
    if (sample.present && sample.initialized) {
        uint16_t current = 0, average = 0, temperature = 0;
        struct Word { uint8_t reg; uint16_t *value; };
        const Word words[] = {
            {0x08, &sample.voltage_mv}, {0x0c, &current}, {0x14, &average},
            {0x2c, &sample.percent}, {0x10, &sample.remaining_mah},
            {0x12, &sample.full_mah}, {0x3c, &sample.design_mah},
            {0x2a, &sample.cycles}, {0x06, &temperature},
        };
        for (const auto &word : words) {
            ret = battery_read_word(word.reg, *word.value);
            if (ret != ESP_OK) return ret;
        }
        // Convert the signed words explicitly, without unsigned wraparound.
        sample.current_ma = current < 0x8000 ? current : int32_t(current) - 65536;
        sample.average_current_ma = average < 0x8000 ? average : int32_t(average) - 65536;
        sample.temperature_c = temperature / 10.0f - 273.15f;
        // Gauge RAM can reset when its supply is interrupted while USB keeps
        // the MCU alive. Reapply the factory profile before publishing values.
        if (sample.design_mah != BATTERY_DESIGN_MAH) {
            battery_profile_ready = false;
            return ESP_ERR_NOT_FINISHED;
        }
        if (sample.percent > 100 || sample.voltage_mv == 0 || sample.full_mah == 0)
            return ESP_ERR_INVALID_RESPONSE;
    }
    // Commit only a complete sample; an I2C error must never become 0%.
    *data = sample;
    return ESP_OK;
}

esp_err_t bsp_wifi_init(void)
{
    if (wifi_ready) return ESP_OK;
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Netif");
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;
    static esp_netif_t *sta, *ap;
    if (!sta) sta = esp_netif_create_default_wifi_sta();
    if (!ap) ap = esp_netif_create_default_wifi_ap();
    if (!sta || !ap) return ESP_ERR_NO_MEM;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "Wi-Fi driver");
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) { esp_wifi_deinit(); return ret; }
    // Connection credentials, scanning and provisioning belong to network_service.
    wifi_ready = true;
    return ESP_OK;
}

static void report(const char *name, esp_err_t result)
{
    if (result == ESP_OK) ESP_LOGI(TAG, "%s ready", name);
    else ESP_LOGW(TAG, "%s unavailable: %s", name, esp_err_to_name(result));
}

esp_err_t bsp_wifi_start(void)
{
    if (!wifi_ready) return ESP_ERR_INVALID_STATE;
    // esp_wifi_start is idempotent when the initialized Wi-Fi driver is running.
    return esp_wifi_start();
}

esp_err_t bsp_init(void)
{
    // Hold the backlight off from the start of board initialization.
    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Backlight initialization");
    esp_err_t ret = nvs_flash_init();
    // Preserve saved data on NVS incompatibility; report instead of silently erasing.
    ESP_RETURN_ON_ERROR(ret, TAG, "NVS initialization");
    if (!bsp_display_start()) {
        if (bsp_board_status.i2c != ESP_OK) return bsp_board_status.i2c;
        return bsp_board_status.display;
    }
    bsp_board_status.button = bsp_user_button_init(); report("BOOT button", bsp_board_status.button);
    bsp_board_status.audio = bsp_extra_codec_init(); report("ES8311 + ES7210", bsp_board_status.audio);
    bsp_board_status.rtc = bsp_rtc_init(); report("PCF85063", bsp_board_status.rtc);
    bsp_board_status.imu = bsp_imu_init(); report("QMI8658", bsp_board_status.imu);
    bsp_board_status.battery = bsp_battery_init(); report("BQ27220", bsp_board_status.battery);
    bsp_board_status.sdcard = bsp_sdcard_mount(); report("SD card", bsp_board_status.sdcard);
    bsp_board_status.wifi = bsp_wifi_init(); report("Wi-Fi driver", bsp_board_status.wifi);
    return ESP_OK;
}
