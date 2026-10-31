#include "ICM20948.h"
#include "BankSelect.h"
#include "CombineBigEndian.h"
#include "ConfigBits.h"
#include "MagBits.h"

namespace {
constexpr uint8_t REG_BANK_SEL = 0x7F;
constexpr uint8_t WHO_AM_I_BANK = 0;
constexpr uint8_t WHO_AM_I_REG = 0x00;
constexpr uint8_t PWR_MGMT_1 = 0x06;
constexpr uint8_t PWR_MGMT_2 = 0x07;
constexpr uint8_t PWR_MGMT_1_DEVICE_RESET = 0x80;
constexpr uint8_t ACCEL_XOUT_H = 0x2D;

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

// I2C master (bank 0 control/status, bank 3 config) for the AK09916.
constexpr uint8_t USER_CTRL = 0x03;
constexpr uint8_t USER_CTRL_I2C_MST_EN = 0x20;
constexpr uint8_t I2C_MST_STATUS = 0x17;
constexpr uint8_t I2C_MST_STATUS_PERIPH4_DONE = 0x40;
constexpr uint8_t I2C_MST_STATUS_PERIPH4_NACK = 0x10;

constexpr uint8_t I2C_MST_BANK = 3;
constexpr uint8_t I2C_MST_ODR_CONFIG = 0x00;
constexpr uint8_t I2C_MST_ODR_137HZ = 0x03;  // 1.1 kHz / 2^3
constexpr uint8_t I2C_MST_CTRL = 0x01;
constexpr uint8_t I2C_MST_CTRL_VALUE = 0x17;  // stop between reads, ~345 kHz
constexpr uint8_t I2C_PERIPH4_ADDR = 0x13;
constexpr uint8_t I2C_PERIPH4_REG = 0x14;
constexpr uint8_t I2C_PERIPH4_CTRL = 0x15;
constexpr uint8_t I2C_PERIPH4_DO = 0x16;
constexpr uint8_t I2C_PERIPH4_DI = 0x17;
constexpr uint8_t I2C_PERIPH4_EN = 0x80;
constexpr uint32_t PERIPH4_TIMEOUT_MS = 20;

constexpr uint8_t I2C_PERIPH0_ADDR = 0x03;
constexpr uint8_t I2C_PERIPH0_REG = 0x04;
constexpr uint8_t I2C_PERIPH0_CTRL = 0x05;

constexpr uint8_t AK09916_ADDRESS = 0x0C;
constexpr uint8_t AK09916_ST1 = 0x10;
constexpr uint8_t AK09916_CNTL2 = 0x31;
constexpr uint8_t AK09916_CNTL3 = 0x32;
constexpr uint8_t AK09916_SOFT_RESET = 0x01;
constexpr uint8_t AK09916_CONTINUOUS_100HZ = 0x08;  // mode 4
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
    wire_->endTransmission(false);  // repeated start
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

void ICM20948::reset() {
    writeRegister(0, PWR_MGMT_1, PWR_MGMT_1_DEVICE_RESET);
    delay(100);  // setup only
}

void ICM20948::wake() {
    writeRegister(0, PWR_MGMT_1, 0x01);  // auto clock, awake, temp on
    writeRegister(0, PWR_MGMT_2, 0x00);  // all axes on
}

void ICM20948::configureSampling() {
    // ODR alignment has to be on before the divider writes.
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

void ICM20948::enableMagBus() {
    writeRegister(I2C_MST_BANK, I2C_MST_ODR_CONFIG, I2C_MST_ODR_137HZ);
    writeRegister(I2C_MST_BANK, I2C_MST_CTRL, I2C_MST_CTRL_VALUE);
    writeRegister(0, USER_CTRL, USER_CTRL_I2C_MST_EN);
    delay(10);  // setup only
}

bool ICM20948::waitPeriph4Done() {
    uint32_t start = millis();
    while (millis() - start < PERIPH4_TIMEOUT_MS) {
        uint8_t status = readRegister(0, I2C_MST_STATUS);
        if (status & I2C_MST_STATUS_PERIPH4_NACK) {
            return false;
        }
        if (status & I2C_MST_STATUS_PERIPH4_DONE) {
            return true;
        }
        delay(1);  // setup only
    }
    return false;
}

bool ICM20948::readMagRegister(uint8_t reg, uint8_t &value) {
    readRegister(0, I2C_MST_STATUS);  // clear stale flags
    writeRegister(I2C_MST_BANK, I2C_PERIPH4_ADDR,
                  i2cPeriphAddrValue(AK09916_ADDRESS, true));
    writeRegister(I2C_MST_BANK, I2C_PERIPH4_REG, reg);
    writeRegister(I2C_MST_BANK, I2C_PERIPH4_CTRL, I2C_PERIPH4_EN);
    if (!waitPeriph4Done()) {
        return false;
    }
    value = readRegister(I2C_MST_BANK, I2C_PERIPH4_DI);
    return true;
}

bool ICM20948::writeMagRegister(uint8_t reg, uint8_t value) {
    readRegister(0, I2C_MST_STATUS);  // clear stale flags
    writeRegister(I2C_MST_BANK, I2C_PERIPH4_ADDR,
                  i2cPeriphAddrValue(AK09916_ADDRESS, false));
    writeRegister(I2C_MST_BANK, I2C_PERIPH4_REG, reg);
    writeRegister(I2C_MST_BANK, I2C_PERIPH4_DO, value);
    writeRegister(I2C_MST_BANK, I2C_PERIPH4_CTRL, I2C_PERIPH4_EN);
    return waitPeriph4Done();
}

bool ICM20948::startMag() {
    if (!writeMagRegister(AK09916_CNTL3, AK09916_SOFT_RESET)) {
        return false;
    }
    delay(10);  // setup only
    if (!writeMagRegister(AK09916_CNTL2, AK09916_CONTINUOUS_100HZ)) {
        return false;
    }
    // Peripheral 0 then reads ST1..ST2 every internal sample and copies it
    // into EXT_PERIPH_SENS_DATA_00, right after TEMP_OUT_L. Reading ST2
    // each time releases the AK09916's data latch.
    writeRegister(I2C_MST_BANK, I2C_PERIPH0_ADDR,
                  i2cPeriphAddrValue(AK09916_ADDRESS, true));
    writeRegister(I2C_MST_BANK, I2C_PERIPH0_REG, AK09916_ST1);
    writeRegister(I2C_MST_BANK, I2C_PERIPH0_CTRL,
                  i2cPeriphCtrlValue(true, MAG_READ_LEN));
    delay(20);  // setup only: let the first mag reads land
    return true;
}

void ICM20948::readSensorBlock(int16_t raw[SAMPLE_RAW_WORDS],
                               uint16_t &magStatus) {
    selectBank(0);
    wire_->beginTransmission(address_);
    wire_->write(ACCEL_XOUT_H);
    wire_->endTransmission(false);  // repeated start
    wire_->requestFrom(address_, static_cast<uint8_t>(SENSOR_BLOCK_LEN));

    uint8_t block[SENSOR_BLOCK_LEN];
    for (size_t i = 0; i < SENSOR_BLOCK_LEN; i++) {
        block[i] = wire_->read();
    }
    decodeSensorBlock(block, raw, magStatus);
}
