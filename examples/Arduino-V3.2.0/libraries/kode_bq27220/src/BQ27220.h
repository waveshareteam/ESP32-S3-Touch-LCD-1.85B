// Minimal Arduino library for TI BQ27220 fuel gauge (I2C)
// NOTE: Register map defaults follow common TI gauges (e.g., bq27441).
//       Adjust the register addresses below to match BQ27220 after reviewing its datasheet.

#pragma once

#include <Arduino.h>
#include <Wire.h>

// Default 7-bit I2C address used by many TI single-cell gauges
#ifndef BQ27220_I2C_ADDRESS
#define BQ27220_I2C_ADDRESS 0x55
#endif

// Register addresses (little-endian 16-bit)
#ifndef BQ27220_REG_TEMPERATURE
#define BQ27220_REG_TEMPERATURE 0x06 // Temperature() 0.1 K
#endif

#ifndef BQ27220_REG_VOLTAGE
#define BQ27220_REG_VOLTAGE 0x08 // Voltage() mV
#endif

#ifndef BQ27220_REG_AVG_CURRENT
#define BQ27220_REG_AVG_CURRENT 0x14 // AverageCurrent() mA, signed
#endif

#ifndef BQ27220_REG_SOC
#define BQ27220_REG_SOC 0x2C // StateOfCharge() %
#endif

#ifndef BQ27220_REG_INT_TEMPERATURE
#define BQ27220_REG_INT_TEMPERATURE 0x28 // InternalTemperature() 0.1 K
#endif

#ifndef BQ27220_REG_CURRENT
#define BQ27220_REG_CURRENT 0x0C // Current() mA, signed
#endif

#ifndef BQ27220_REG_REMAINING_CAPACITY
#define BQ27220_REG_REMAINING_CAPACITY 0x10 // mAh
#endif

#ifndef BQ27220_REG_FULL_CHARGE_CAPACITY
#define BQ27220_REG_FULL_CHARGE_CAPACITY 0x12 // mAh
#endif

#ifndef BQ27220_REG_STANDBY_CURRENT
#define BQ27220_REG_STANDBY_CURRENT 0x1A // mA
#endif

#ifndef BQ27220_REG_TIME_TO_EMPTY
#define BQ27220_REG_TIME_TO_EMPTY 0x16 // minutes
#endif

#ifndef BQ27220_REG_TIME_TO_FULL
#define BQ27220_REG_TIME_TO_FULL 0x18 // minutes
#endif

#ifndef BQ27220_REG_CYCLE_COUNT
#define BQ27220_REG_CYCLE_COUNT 0x2A // cycles
#endif

#ifndef BQ27220_REG_SOH
#define BQ27220_REG_SOH 0x2E // %
#endif

#ifndef BQ27220_REG_BATTERY_STATUS
#define BQ27220_REG_BATTERY_STATUS 0x0A // flags
#endif

#ifndef BQ27220_REG_OPERATION_STATUS
#define BQ27220_REG_OPERATION_STATUS 0x3A // flags
#endif

// Control() subcommands written to 0x00/0x01
#ifndef BQ27220_CTRL_RESET
#define BQ27220_CTRL_RESET 0x0041
#endif

#ifndef BQ27220_CTRL_SEALED
#define BQ27220_CTRL_SEALED 0x0030
#endif

#ifndef BQ27220_CTRL_UNSEAL_KEY1
#define BQ27220_CTRL_UNSEAL_KEY1 0x0414
#endif

#ifndef BQ27220_CTRL_UNSEAL_KEY2
#define BQ27220_CTRL_UNSEAL_KEY2 0x3672
#endif

#ifndef BQ27220_CTRL_FULL_ACCESS
#define BQ27220_CTRL_FULL_ACCESS 0xFFFF
#endif

// Data Memory (DM) addresses used commonly
#ifndef BQ27220_DM_ADDR_PROFILE1_DESIGN_CAPACITY
#define BQ27220_DM_ADDR_PROFILE1_DESIGN_CAPACITY 0x929F
#endif
#ifndef BQ27220_DM_ADDR_PROFILE1_EDV0
#define BQ27220_DM_ADDR_PROFILE1_EDV0 0x92B4
#endif
#ifndef BQ27220_DM_ADDR_PROFILE1_EDV1
#define BQ27220_DM_ADDR_PROFILE1_EDV1 0x92B7
#endif
#ifndef BQ27220_DM_ADDR_PROFILE1_EDV2
#define BQ27220_DM_ADDR_PROFILE1_EDV2 0x92BA
#endif

class BQ27220 {
public:
  BQ27220();

  // Initialize with a TwoWire bus and optional custom address
  bool begin(TwoWire &wire = Wire, uint8_t i2cAddress = BQ27220_I2C_ADDRESS,
             int sdaPin = -1, int sclPin = -1, uint32_t frequency = 400000U);

  // Basic measurements
  // Returns temperature in 0.1 K units (or NAN on failure)
  float readTemperatureCelsius();
  // Returns voltage in millivolts (or -1 on failure)
  int readVoltageMillivolts();
  // Returns average current in milliamps (signed). Sign depends on system; examples assume positive=charging. INT16_MIN on failure
  int readAverageCurrentMilliamps();
  // Returns instantaneous current in milliamps (signed). Sign depends on system. INT16_MIN on failure
  int readCurrentMilliamps();
  // Returns state of charge in % (0..100). -1 on failure
  int readStateOfChargePercent();
  // Capacity and counters
  int readRemainingCapacitymAh();
  int readFullChargeCapacitymAh();
  int readDesignCapacitymAh();
  int readCycleCount();
  int readStateOfHealthPercent();
  // Status registers
  bool readBatteryStatus(uint16_t &status);
  bool readOperationStatus(uint16_t &status);
  // Time estimates
  int readTimeToEmptyMinutes();
  int readTimeToFullMinutes();

  // Low-level register reads
  bool readWord(uint8_t reg, uint16_t &value);
  bool readWordSigned(uint8_t reg, int16_t &value);
  bool writeWord(uint8_t reg, uint16_t value);

  // Control() helpers
  bool reset();
  bool seal();
  bool unseal();
  bool fullAccess();
  bool setDesignCapacity(uint16_t capacitymAh);
  // Config-update session for writing Data Memory (CEDV params, EDVs...)
  bool beginConfigUpdate();
  bool endConfigUpdate(bool reinit = false);
  bool writeDataMemory(uint16_t address, const uint8_t* data, uint8_t length);
  bool writeDataMemoryU16(uint16_t address, uint16_t value);
  bool setEDVRawU16(uint16_t edv0, uint16_t edv1, uint16_t edv2);
  // Convenience: set EDVs using millivolts (writes U16 values to DM). Consult TRM if scaling differs.
  bool setEDVsMillivolts(uint16_t edv0mV, uint16_t edv1mV, uint16_t edv2mV);

  // Convenience: initialize safely (unseal, full access, optional reset probe)
  bool initSafe(bool doResetIfNeeded = true);

  void setI2CAddress(uint8_t address);

private:
  TwoWire *wireBus;
  uint8_t deviceAddress;
  bool i2cRead16(uint8_t reg, uint8_t *lsb, uint8_t *msb);
  bool writeControl(uint16_t subcommand);

  // Helpers to detect INITCOMP/CFG states can be added later
};

