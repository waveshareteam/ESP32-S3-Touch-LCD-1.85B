#include <Wire.h>
#include <BQ27220.h>

// Optional for ESP32-S3 custom I2C pins
#if defined(ARDUINO_ARCH_ESP32)
#ifndef SDA_PIN
#define SDA_PIN 48
#endif
#ifndef SCL_PIN
#define SCL_PIN 47
#endif
#endif

BQ27220 gauge;

void setup() {
  Serial.begin(115200);

  // Start I2C and the gauge
#if defined(ARDUINO_ARCH_ESP32)
  // ESP32-S3: SDA=48, SCL=47 (adjust to your wiring)
  if (!gauge.begin(Wire, 0x55, SDA_PIN, SCL_PIN, 400000)) {
    Serial.println("BQ27220 not found");
    while (1) delay(1000);
  }
#else
  // Other boards: use default Wire pins
  if (!gauge.begin()) {
    Serial.println("BQ27220 not found");
    while (1) delay(1000);
  }
#endif

  Serial.println("BQ27220 ready");
}

void loop() {
  int soc = gauge.readStateOfChargePercent();
  int mv = gauge.readVoltageMillivolts();
  int ma = gauge.readCurrentMilliamps();      // positive = charging
  float tC = gauge.readTemperatureCelsius();

  Serial.print("SOC= "); Serial.print(soc); Serial.print("%  ");
  Serial.print("V= "); Serial.print(mv); Serial.print(" mV  ");
  Serial.print("I= "); Serial.print(ma); Serial.print(" mA  ");
  Serial.print("T= "); Serial.print(tC, 1); Serial.print(" C");
  if (ma > 0) { // charging (observed sign)
    int ttf = gauge.readTimeToFullMinutes();
    Serial.print("  TTF= "); Serial.print(ttf); Serial.print(" min");
  }
  Serial.println();

  delay(1000);
}

