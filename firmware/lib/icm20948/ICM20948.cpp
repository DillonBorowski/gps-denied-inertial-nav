#include "ICM20948.h"
#include "BankSelect.h"
#include "CombineBigEndian.h"

namespace {
constexpr uint8_t REG_BANK_SEL = 0x7F;
constexpr uint8_t WHO_AM_I_BANK = 0;
constexpr uint8_t WHO_AM_I_REG = 0x00;
constexpr uint8_t PWR_MGMT_1 = 0x06;
constexpr uint8_t PWR_MGMT_2 = 0x07;
constexpr uint8_t ACCEL_XOUT_H = 0x2D;
constexpr uint8_t ACCEL_GYRO_BLOCK_LEN = 12;
}  // namespace

void ICM20948::begin(TwoWire &wire, uint8_t address) {
    wire_ = &wire;
    address_ = address;
}

void ICM20948::selectBank(uint8_t bank) {
    wire_->beginTransmission(address_);
    wire_->write(REG_BANK_SEL);
    wire_->write(bankSelectValue(bank));
    wire_->endTransmission();
}

uint8_t ICM20948::readRegister(uint8_t bank, uint8_t reg) {
    selectBank(bank);
    wire_->beginTransmission(address_);
    wire_->write(reg);
    wire_->endTransmission(false);  // repeated start, keep the bus held
    wire_->requestFrom(address_, static_cast<uint8_t>(1));
    return wire_->read();
}

void ICM20948::writeRegister(uint8_t bank, uint8_t reg, uint8_t value) {
    selectBank(bank);
    wire_->beginTransmission(address_);
    wire_->write(reg);
    wire_->write(value);
    wire_->endTransmission();
}

uint8_t ICM20948::readWhoAmI() {
    return readRegister(WHO_AM_I_BANK, WHO_AM_I_REG);
}

void ICM20948::wake() {
    // CLKSEL=1 (auto-select best clock), SLEEP/LP_EN/TEMP_DIS cleared.
    writeRegister(0, PWR_MGMT_1, 0x01);
    // All accel/gyro axes enabled (disable bits cleared).
    writeRegister(0, PWR_MGMT_2, 0x00);
}

void ICM20948::readAccelGyroRaw(int16_t out[6]) {
    selectBank(0);
    wire_->beginTransmission(address_);
    wire_->write(ACCEL_XOUT_H);
    wire_->endTransmission(false);  // repeated start, keep the bus held
    wire_->requestFrom(address_, ACCEL_GYRO_BLOCK_LEN);

    uint8_t raw[ACCEL_GYRO_BLOCK_LEN];
    for (uint8_t i = 0; i < ACCEL_GYRO_BLOCK_LEN; i++) {
        raw[i] = wire_->read();
    }

    for (uint8_t axis = 0; axis < 6; axis++) {
        out[axis] = combineBigEndian(raw[axis * 2], raw[axis * 2 + 1]);
    }
}
