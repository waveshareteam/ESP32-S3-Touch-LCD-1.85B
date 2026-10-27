#include <Wire.h>
#include <BQ27220.h>

#if defined(ARDUINO_ARCH_ESP32)
#ifndef SDA_PIN
#define SDA_PIN 48
#endif
#ifndef SCL_PIN
#define SCL_PIN 47
#endif
#endif

// Example: Set design capacity to 500 mAh and (optionally) update EDV thresholds
// WARNING: Writing Data Memory requires understanding the device TRM. Use at your own risk.

static const uint16_t DESIGN_CAPACITY_MAH = 500;
// Example raw EDV values (placeholders!). Refer to TRM for scaling/units if needed.
// If you don't know these yet, comment setEDVRawU16() call below.
// static const uint16_t EDV0_RAW = 0x0C80; // placeholder
// static const uint16_t EDV1_RAW = 0x0D00; // placeholder
// static const uint16_t EDV2_RAW = 0x0D40; // placeholder

BQ27220 gauge;

void halt(const char* msg) {
  Serial.println(msg);
  while (1) delay(1000);
}

void setup() {
  Serial.begin(115200);

#if defined(ARDUINO_ARCH_ESP32)
  if (!gauge.begin(Wire, 0x55, SDA_PIN, SCL_PIN, 400000)) halt("BQ27220 not found");
#else
  if (!gauge.begin()) halt("BQ27220 not found");
#endif

  Serial.println("Connected to BQ27220");

  // Unseal and get full access
  if (!gauge.unseal()) Serial.println("Warning: unseal() may have failed (device could be already unsealed)");
  if (!gauge.fullAccess()) Serial.println("Warning: fullAccess() may have failed (device could be already full access)");

  // Set Command DesignCapacity (0x3C)
  if (!gauge.setDesignCapacity(DESIGN_CAPACITY_MAH)) halt("Failed to set design capacity");
  Serial.print("Design capacity set to "); Serial.print(DESIGN_CAPACITY_MAH); Serial.println(" mAh");

  // Enter CONFIG_UPDATE to write Data Memory (optional EDV tuning)
  if (!gauge.beginConfigUpdate()) halt("Failed to enter CONFIG_UPDATE");

  // Example: write profile1 design capacity to DM too (not strictly required when using 0x3C)
  if (!gauge.writeDataMemoryU16(BQ27220_DM_ADDR_PROFILE1_DESIGN_CAPACITY, DESIGN_CAPACITY_MAH))
    Serial.println("Warning: could not write DM design capacity");

  // OPTIONAL: write raw EDV values (requires correct raw format per TRM)
  // Comment the next block if you don't know your EDVs yet
  // if (!gauge.setEDVRawU16(EDV0_RAW, EDV1_RAW, EDV2_RAW))
  //   Serial.println("Warning: could not write EDV values to DM");

  if (!gauge.endConfigUpdate(true)) halt("Failed to exit CONFIG_UPDATE");

  // Optionally seal device when done
  if (!gauge.seal()) Serial.println("Warning: seal() failed (device could be already sealed)");

  Serial.println("Config update complete");
}

void loop() {
  int soc = gauge.readStateOfChargePercent();
  int mv = gauge.readVoltageMillivolts();
  int ma = gauge.readAverageCurrentMilliamps();
  Serial.print("SOC="); Serial.print(soc); Serial.print("%  V="); Serial.print(mv);
  Serial.print(" mV  Iavg="); Serial.print(ma); Serial.println(" mA");
  delay(1000);
}

