#include "BQ27220.h"

BQ27220::BQ27220() : wireBus(nullptr), deviceAddress(BQ27220_I2C_ADDRESS) {}

bool BQ27220::begin(TwoWire &wire, uint8_t i2cAddress, int sdaPin, int sclPin,
                    uint32_t frequency) {
  wireBus = &wire;
  deviceAddress = i2cAddress;

  // If custom pins were provided, initialize bus with pins; otherwise regular begin
#if defined(ARDUINO_ARCH_ESP32)
  if (sdaPin >= 0 && sclPin >= 0) {
    wireBus->begin(sdaPin, sclPin, frequency);
  } else {
    wireBus->begin();
    wireBus->setClock(frequency);
  }
#else
  (void)sdaPin;
  (void)sclPin;
  (void)frequency;
  wireBus->begin();
#endif

  // Quick probe by reading voltage
  int mv = readVoltageMillivolts();
  return mv >= 0;
}

void BQ27220::setI2CAddress(uint8_t address) { deviceAddress = address; }

bool BQ27220::i2cRead16(uint8_t reg, uint8_t *lsb, uint8_t *msb) {
  if (wireBus == nullptr) return false;

  wireBus->beginTransmission(deviceAddress);
  wireBus->write(reg);
  if (wireBus->endTransmission(false) != 0) return false; // repeated start

  int requested = wireBus->requestFrom((int)deviceAddress, 2);
  if (requested != 2) return false;
  if (!wireBus->available()) return false;
  *lsb = (uint8_t)wireBus->read();
  if (!wireBus->available()) return false;
  *msb = (uint8_t)wireBus->read();
  return true;
}

bool BQ27220::readWord(uint8_t reg, uint16_t &value) {
  uint8_t lsb = 0, msb = 0;
  if (!i2cRead16(reg, &lsb, &msb)) return false;
  value = (uint16_t)lsb | ((uint16_t)msb << 8); // little-endian
  return true;
}

bool BQ27220::readWordSigned(uint8_t reg, int16_t &value) {
  uint16_t raw = 0;
  if (!readWord(reg, raw)) return false;
  value = (int16_t)raw;
  return true;
}

bool BQ27220::writeWord(uint8_t reg, uint16_t value) {
  if (wireBus == nullptr) return false;
  wireBus->beginTransmission(deviceAddress);
  wireBus->write(reg);
  wireBus->write((uint8_t)(value & 0xFF));
  wireBus->write((uint8_t)((value >> 8) & 0xFF));
  return wireBus->endTransmission() == 0;
}

float BQ27220::readTemperatureCelsius() {
  uint16_t raw = 0;
  if (!readWord(BQ27220_REG_TEMPERATURE, raw)) return NAN;
  float kelvin = raw / 10.0f; // 0.1 K
  return kelvin - 273.15f;
}

int BQ27220::readVoltageMillivolts() {
  uint16_t mv = 0;
  if (!readWord(BQ27220_REG_VOLTAGE, mv)) return -1;
  return (int)mv;
}

int BQ27220::readAverageCurrentMilliamps() {
  int16_t ma = 0;
  if (!readWordSigned(BQ27220_REG_AVG_CURRENT, ma)) return INT16_MIN;
  return (int)ma;
}

int BQ27220::readCurrentMilliamps() {
  int16_t ma = 0;
  if (!readWordSigned(BQ27220_REG_CURRENT, ma)) return INT16_MIN;
  return (int)ma;
}

int BQ27220::readStateOfChargePercent() {
  uint16_t soc = 0;
  if (!readWord(BQ27220_REG_SOC, soc)) return -1;
  return (int)soc;
}

int BQ27220::readRemainingCapacitymAh() {
  uint16_t v = 0;
  if (!readWord(BQ27220_REG_REMAINING_CAPACITY, v)) return -1;
  return (int)v;
}

int BQ27220::readFullChargeCapacitymAh() {
  uint16_t v = 0;
  if (!readWord(BQ27220_REG_FULL_CHARGE_CAPACITY, v)) return -1;
  return (int)v;
}

int BQ27220::readDesignCapacitymAh() {
  uint16_t v = 0;
  if (!readWord(0x3C, v)) return -1; // CommandDesignCapacity
  return (int)v;
}

int BQ27220::readCycleCount() {
  uint16_t v = 0;
  if (!readWord(BQ27220_REG_CYCLE_COUNT, v)) return -1;
  return (int)v;
}

int BQ27220::readStateOfHealthPercent() {
  uint16_t v = 0;
  if (!readWord(BQ27220_REG_SOH, v)) return -1;
  return (int)v;
}

bool BQ27220::readBatteryStatus(uint16_t &status) { return readWord(BQ27220_REG_BATTERY_STATUS, status); }

bool BQ27220::readOperationStatus(uint16_t &status) { return readWord(BQ27220_REG_OPERATION_STATUS, status); }

int BQ27220::readTimeToEmptyMinutes() {
  uint16_t v = 0;
  if (!readWord(BQ27220_REG_TIME_TO_EMPTY, v)) return -1;
  return (int)v; // minutes
}

int BQ27220::readTimeToFullMinutes() {
  uint16_t v = 0;
  if (!readWord(BQ27220_REG_TIME_TO_FULL, v)) return -1;
  return (int)v; // minutes
}

bool BQ27220::writeControl(uint16_t subcommand) {
  if (wireBus == nullptr) return false;
  // Control() is at 0x00/0x01. Write little-endian 16-bit value across two addresses.
  // Write LSB address 0x00 and data LSB
  wireBus->beginTransmission(deviceAddress);
  wireBus->write((uint8_t)0x00);
  wireBus->write((uint8_t)(subcommand & 0xFF));
  if (wireBus->endTransmission() != 0) return false;
  // Write MSB address 0x01 and data MSB
  wireBus->beginTransmission(deviceAddress);
  wireBus->write((uint8_t)0x01);
  wireBus->write((uint8_t)((subcommand >> 8) & 0xFF));
  return wireBus->endTransmission() == 0;
}

bool BQ27220::reset() { return writeControl(BQ27220_CTRL_RESET); }

bool BQ27220::seal() { return writeControl(BQ27220_CTRL_SEALED); }

bool BQ27220::unseal() {
  if (!writeControl(BQ27220_CTRL_UNSEAL_KEY1)) return false;
  delay(2);
  return writeControl(BQ27220_CTRL_UNSEAL_KEY2);
}

bool BQ27220::fullAccess() { return writeControl(BQ27220_CTRL_FULL_ACCESS); }

bool BQ27220::setDesignCapacity(uint16_t capacitymAh) {
  // Requires unsealed or full access; write to CommandDesignCapacity (0x3C)
  return writeWord(0x3C, capacitymAh);
}

// -------------------- CONFIG UPDATE HELPERS --------------------
// Subcommand headers: 0x0090 enter, 0x0092 exit, 0x0091 exit+reinit
bool BQ27220::beginConfigUpdate() { return writeControl(0x0090); }

bool BQ27220::endConfigUpdate(bool reinit) {
  return writeControl(reinit ? 0x0091 : 0x0092);
}

// Generic DM write via subcommand buffer @ 0x3E/0x3F + 0x40..0x5F + checksum@0x60 len@0x61
bool BQ27220::writeDataMemory(uint16_t address, const uint8_t* data, uint8_t length) {
  if (wireBus == nullptr || data == nullptr || length == 0 || length > 32) return false;

  // 1) Write address (little-endian) into 0x3E/0x3F
  wireBus->beginTransmission(deviceAddress);
  wireBus->write((uint8_t)0x3E);
  wireBus->write((uint8_t)(address & 0xFF));
  if (wireBus->endTransmission() != 0) return false;

  wireBus->beginTransmission(deviceAddress);
  wireBus->write((uint8_t)0x3F);
  wireBus->write((uint8_t)((address >> 8) & 0xFF));
  if (wireBus->endTransmission() != 0) return false;

  delay(2);

  // 2) Write data buffer into 0x40.. (up to 32 bytes)
  // We write sequentially starting at 0x40
  wireBus->beginTransmission(deviceAddress);
  wireBus->write((uint8_t)0x40);
  for (uint8_t i = 0; i < length; ++i) wireBus->write(data[i]);
  if (wireBus->endTransmission() != 0) return false;

  // 3) Compute checksum: bitwise inverted 8-bit sum of [0x3E,0x3F,<data...>]
  uint8_t sum = 0;
  sum += (uint8_t)(address & 0xFF);
  sum += (uint8_t)((address >> 8) & 0xFF);
  for (uint8_t i = 0; i < length; ++i) sum += data[i];
  uint8_t csum = (uint8_t)~sum;

  // 4) Write checksum @0x60 and length+4 at @0x61 as a word write
  wireBus->beginTransmission(deviceAddress);
  wireBus->write((uint8_t)0x60);
  wireBus->write(csum);
  // length field includes 0x3E,0x3F and 0x60,0x61 plus payload
  uint8_t totalLen = (uint8_t)(length + 4);
  wireBus->write(totalLen);
  if (wireBus->endTransmission() != 0) return false;

  // 5) Small delay for commit
  delay(4);
  return true;
}

bool BQ27220::writeDataMemoryU16(uint16_t address, uint16_t value) {
  uint8_t buf[2] = {(uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF)};
  return writeDataMemory(address, buf, 2);
}

bool BQ27220::setEDVRawU16(uint16_t edv0, uint16_t edv1, uint16_t edv2) {
  bool ok = true;
  ok &= writeDataMemoryU16(BQ27220_DM_ADDR_PROFILE1_EDV0, edv0);
  ok &= writeDataMemoryU16(BQ27220_DM_ADDR_PROFILE1_EDV1, edv1);
  ok &= writeDataMemoryU16(BQ27220_DM_ADDR_PROFILE1_EDV2, edv2);
  return ok;
}

bool BQ27220::setEDVsMillivolts(uint16_t edv0mV, uint16_t edv1mV, uint16_t edv2mV) {
  // NOTE: Many TI gauges store raw EDV thresholds in mV units as U16. If your device/TRM uses a different scaling,
  // convert here before writing. For now we write the mV value directly.
  return setEDVRawU16(edv0mV, edv1mV, edv2mV);
}

bool BQ27220::initSafe(bool doResetIfNeeded) {
  // Attempt to unseal and get full access; device might already be in that state
  unseal();
  fullAccess();

  // Quick probe
  int mv = readVoltageMillivolts();
  if (mv < 0 && doResetIfNeeded) {
    // If probe failed, try a soft reset and probe once more
    reset();
    delay(100);
    mv = readVoltageMillivolts();
  }
  return mv >= 0;
}

