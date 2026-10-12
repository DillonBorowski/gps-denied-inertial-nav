// Phase 1 sanity check: bank-select mechanism + WHO_AM_I read.
// Per docs/icm20948-register-map.md this must be verified before touching
// any config registers.

#include <Arduino.h>
#include "ICM20948.h"

static const int PIN_SDA = 8;
static const int PIN_SCL = 9;
static const uint8_t ICM20948_ADDRESS = 0x69;
static const uint8_t EXPECTED_WHO_AM_I = 0xEA;

ICM20948 imu;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  delay(500);

  Wire.begin(PIN_SDA, PIN_SCL);
  imu.begin(Wire, ICM20948_ADDRESS);

  uint8_t whoAmI = imu.readWhoAmI();
  Serial.printf("WHO_AM_I = 0x%02X (expected 0x%02X) -- %s\n",
                whoAmI, EXPECTED_WHO_AM_I,
                whoAmI == EXPECTED_WHO_AM_I ? "PASS" : "FAIL");

  imu.wake();
}

void loop() {
  int16_t raw[6];
  imu.readAccelGyroRaw(raw);
  Serial.printf("ax=%d ay=%d az=%d gx=%d gy=%d gz=%d\n",
                raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
  delay(2000);
}
