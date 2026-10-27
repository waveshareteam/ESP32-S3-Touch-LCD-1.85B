#pragma once
#include "bsp/esp-bsp.h"

enum class BatteryState { Reading, Unavailable, NoBattery, Initializing, Idle, Charging, Discharging, Full };
struct BatterySnapshot {
    bsp_battery_data_t data = {};
    esp_err_t error = ESP_ERR_NOT_FINISHED;
    int64_t sampled_us = 0;
};

// Both consumers apply the same freshness and charge-state rules.
inline constexpr BatteryState battery_state(const BatterySnapshot &s, int64_t now_us)
{
    if (s.error == ESP_ERR_NOT_FINISHED) return BatteryState::Reading;
    if (s.error != ESP_OK || s.sampled_us <= 0 || now_us < s.sampled_us ||
        now_us - s.sampled_us > 3000000) return BatteryState::Unavailable;
    if (!s.data.present) return BatteryState::NoBattery;
    if (!s.data.initialized) return BatteryState::Initializing;
    // Charge/discharge is measured current, not merely the presence of USB.
    if (s.data.current_ma > 5) return BatteryState::Charging;
    if (s.data.current_ma < -5) return BatteryState::Discharging;
    if (s.data.battery_status & (1u << 9)) return BatteryState::Full;
    return BatteryState::Idle;
}
inline constexpr bool battery_has_values(BatteryState state)
{
    return state == BatteryState::Charging || state == BatteryState::Discharging ||
           state == BatteryState::Full || state == BatteryState::Idle;
}
inline const char *battery_state_text(BatteryState state)
{
    switch (state) {
    case BatteryState::Reading: return "Reading gauge...";
    case BatteryState::Unavailable: return "Gauge unavailable";
    case BatteryState::NoBattery: return "No battery detected";
    case BatteryState::Initializing: return "Gauge initializing";
    case BatteryState::Charging: return "Charging";
    case BatteryState::Discharging: return "Discharging";
    case BatteryState::Full: return "Fully charged";
    default: return "Idle";
    }
}
bool battery_service_start(); // Once at startup; independent of the APP lifecycle.
BatterySnapshot battery_service_snapshot(); // Cache only; never blocks on I2C.
