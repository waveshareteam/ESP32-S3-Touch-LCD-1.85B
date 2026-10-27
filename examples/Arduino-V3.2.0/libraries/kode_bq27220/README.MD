# BQ27220_Arduino

Minimal-yet-practical Arduino library for the TI BQ27220 (CEDV) single‑cell Li‑ion/Li‑Po fuel gauge.

- Simple read API (Voltage, Current, SOC, Temperature, capacities, SOH, status)
- Works with the default device profile (no CEDV programming required to start)
- Prepared for advanced configuration (CONFIG_UPDATE, Data Memory writes, EDV tuning) when you need it
- Examples included: Basic and Advanced configuration

## Quick start

- I2C address: 0x55 (7‑bit)
- ESP32‑S3 example pins: SDA=GPIO48, SCL=GPIO47

```cpp
#include <Wire.h>
#include <BQ27220.h>

BQ27220 gauge;

void setup() {
  Serial.begin(115200);
  gauge.begin(Wire, 0x55, 48, 47, 400000);
}

void loop() {
  int soc = gauge.readStateOfChargePercent();
  int mv  = gauge.readVoltageMillivolts();
  int ma  = gauge.readCurrentMilliamps();   // charging is positive
  float t = gauge.readTemperatureCelsius();
  Serial.printf("%d%% | %dmV | %dmA | %.1f C\n", soc, mv, ma, t);
  delay(1000);
}
```

See `examples/Basic_BQ27220/`.

## Current sign convention

In this project/setup, charging shows as positive current and discharging as negative. If your setup is opposite, just negate the value in your sketch. The library returns the raw signed register.

## API overview

- Init
  - `bool begin(TwoWire& wire = Wire, uint8_t addr = 0x55, int sda=-1, int scl=-1, uint32_t freq=400000)`
  - `void setI2CAddress(uint8_t)`
- Measurements
  - `int  readVoltageMillivolts()`
  - `int  readCurrentMilliamps()` (instant)
  - `int  readAverageCurrentMilliamps()`
  - `int  readStateOfChargePercent()`
  - `float readTemperatureCelsius()`
  - `int  readRemainingCapacitymAh()`
  - `int  readFullChargeCapacitymAh()`
  - `int  readDesignCapacitymAh()`
  - `int  readCycleCount()`
  - `int  readStateOfHealthPercent()`
  - `bool readBatteryStatus(uint16_t& flags)`
  - `bool readOperationStatus(uint16_t& flags)`
- Control
  - `bool reset()`
  - `bool unseal()` / `bool fullAccess()` / `bool seal()`
- Simple configuration
  - `bool setDesignCapacity(uint16_t mAh)` (Command 0x3C)
- Advanced configuration (prepared)
  - `bool beginConfigUpdate()` / `bool endConfigUpdate(bool reinit)`
  - `bool writeDataMemory(uint16_t addr, const uint8_t* data, uint8_t len)`
  - `bool writeDataMemoryU16(uint16_t addr, uint16_t value)`
  - `bool setEDVRawU16(uint16_t edv0, uint16_t edv1, uint16_t edv2)`

The advanced API is a thin helper for BQ27220 Data Memory writes using the subcommand buffer (0x3E/0x3F, 0x40.., checksum 0x60, len 0x61) and the CONFIG_UPDATE flow. It is intended for users who later need to program a CEDV profile or EDV thresholds.

## Examples

- `examples/Basic_BQ27220` – minimal read loop for SOC, V, I, T.
- `examples/Advanced_ConfigUpdate_EDV` – shows how to:
  - unseal + fullAccess,
  - set DesignCapacity to 500 mAh,
  - enter CONFIG_UPDATE and write DM (DesignCapacity profile1),
  - optionally write EDV raw values (left commented by default),
  - exit CONFIG_UPDATE and seal.

## Notes

- The library works with the device’s default profile. For many applications, setting DesignCapacity is enough to start.
- For higher accuracy, generate a CEDV profile with TI bqStudio + EV2400 and write Data Memory. The helper methods above are provided, but you should follow TI’s TRM for scaling and timing.

