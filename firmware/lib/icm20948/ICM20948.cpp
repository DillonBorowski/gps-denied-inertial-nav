#include "ICM20948.h"
#include "BankSelect.h"
#include "CombineBigEndian.h"
#include "ConfigBits.h"

namespace {
constexpr uint8_t REG_BANK_SEL = 0x7F;
constexpr uint8_t WHO_AM_I_BANK = 0;
constexpr uint8_t WHO_AM_I_REG = 0x00;
constexpr uint8_t PWR_MGMT_1 = 0x06;
constexpr uint8_t PWR_MGMT_2 = 0x07;
constexpr uint8_t ACCEL_XOUT_H = 0x2D;
constexpr uint8_t ACCEL_GYRO_TEMP_BLOCK_LEN = 14;
constexpr uint8_t ACCEL_GYRO_TEMP_WORDS = 7;

constexpr uint8_t CONFIG_BANK = 2;
constexpr uint8_t GYRO_SMPLRT_DIV = 0x00;
constexpr uint8_t GYRO_CONFIG_1 = 0x01;
constexpr uint8_t ODR_ALIGN_EN = 0x09;
constexpr uint8_t ACCEL_SMPLRT_DIV_1 = 0x10;
constexpr uint8_t ACCEL_SMPLRT_DIV_2 = 0x11;
constexpr uint8_t ACCEL_CONFIG = 0x14;

constexpr uint8_t DLPFCFG_51HZ = 3;  // gyro 51.2 Hz / accel 50.4 Hz 3dB
constexpr uint8_t GYRO_FS_250DPS = 0;
constexpr uint8_t ACCEL_FS_2G = 0;
constexpr uint8_t SMPLRT_DIV_FULL_RATE = 0;
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

void ICM20948::configureSampling() {
    // Alignment must be enabled before the divider writes it keys off.
    writeRegister(CONFIG_BANK, ODR_ALIGN_EN, 0x01);
    writeRegister(CONFIG_BANK, GYRO_CONFIG_1,
                  filterConfigValue(DLPFCFG_51HZ, GYRO_FS_250DPS, true));
    writeRegister(CONFIG_BANK, ACCEL_CONFIG,
                  filterConfigValue(DLPFCFG_51HZ, ACCEL_FS_2G, true));
    writeRegister(CONFIG_BANK, GYRO_SMPLRT_DIV, SMPLRT_DIV_FULL_RATE);
    writeRegister(CONFIG_BANK, ACCEL_SMPLRT_DIV_1,
                  accelSmplrtDivHigh(SMPLRT_DIV_FULL_RATE));
    writeRegister(CONFIG_BANK, ACCEL_SMPLRT_DIV_2,
                  accelSmplrtDivLow(SMPLRT_DIV_FULL_RATE));
}

void ICM20948::readAccelGyroTempRaw(int16_t out[7]) {
    selectBank(0);
    wire_->beginTransmission(address_);
    wire_->write(ACCEL_XOUT_H);
    wire_->endTransmission(false);  // repeated start, keep the bus held
    wire_->requestFrom(address_, ACCEL_GYRO_TEMP_BLOCK_LEN);

    uint8_t raw[ACCEL_GYRO_TEMP_BLOCK_LEN];
    for (uint8_t i = 0; i < ACCEL_GYRO_TEMP_BLOCK_LEN; i++) {
        raw[i] = wire_->read();
    }

    for (uint8_t w = 0; w < ACCEL_GYRO_TEMP_WORDS; w++) {
        out[w] = combineBigEndian(raw[w * 2], raw[w * 2 + 1]);
    }
}
