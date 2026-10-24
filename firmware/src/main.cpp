// Phase 1: hardware-timer-driven, timestamped accel/gyro/temp sampling.
//
// A hardware timer fires at SAMPLE_RATE_HZ. Its ISR only captures the time
// and tick number and wakes the sampler task, which does the I2C burst read
// (I2C is not safe inside an ISR). Samples go through a queue to loop(),
// which streams them as CSV over USB, so a stalled serial port can't delay
// sampling. A gap in seq on the host means a sample was missed or dropped;
// analysis/capture_imu.py reports both and the interval jitter.

#include <Arduino.h>
#include <esp_timer.h>
#include "ICM20948.h"
#include "SampleFormat.h"

static const int PIN_SDA = 8;
static const int PIN_SCL = 9;
static const uint32_t I2C_CLOCK_HZ = 400000;
static const uint8_t ICM20948_ADDRESS = 0x69;
static const uint8_t EXPECTED_WHO_AM_I = 0xEA;

static const uint32_t SAMPLE_RATE_HZ = 200;
static const uint32_t TIMER_TICK_HZ = 1000000;  // 80 MHz APB / 80
static const uint16_t TIMER_DIVIDER = 80;
static const uint8_t SAMPLE_TIMER_NUM = 0;
static const UBaseType_t SAMPLE_QUEUE_LEN = 64;  // ~0.3 s of slack at 200 Hz

ICM20948 imu;

static hw_timer_t *sampleTimer = nullptr;
static TaskHandle_t samplerTask = nullptr;
static QueueHandle_t sampleQueue = nullptr;

static portMUX_TYPE tickMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t tickSeq = 0;
static volatile uint64_t tickTimeUs = 0;

static void IRAM_ATTR onSampleTimer() {
  uint64_t now = esp_timer_get_time();
  portENTER_CRITICAL_ISR(&tickMux);
  tickSeq = tickSeq + 1;
  tickTimeUs = now;
  portEXIT_CRITICAL_ISR(&tickMux);

  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(samplerTask, &woken);
  if (woken) {
    portYIELD_FROM_ISR();
  }
}

static void samplerLoop(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // If this task ran late, ticks coalesce and seq jumps; the host sees
    // that as missed samples rather than silently wrong timing.
    Sample s;
    portENTER_CRITICAL(&tickMux);
    s.seq = tickSeq;
    s.tUs = tickTimeUs;
    portEXIT_CRITICAL(&tickMux);

    imu.readAccelGyroTempRaw(s.raw);
    xQueueSend(sampleQueue, &s, 0);  // full queue drops; seq shows the gap
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  delay(500);

  Wire.begin(PIN_SDA, PIN_SCL, I2C_CLOCK_HZ);
  imu.begin(Wire, ICM20948_ADDRESS);

  uint8_t whoAmI = imu.readWhoAmI();
  Serial.printf("# WHO_AM_I = 0x%02X (expected 0x%02X) -- %s\n",
                whoAmI, EXPECTED_WHO_AM_I,
                whoAmI == EXPECTED_WHO_AM_I ? "PASS" : "FAIL");

  imu.wake();
  delay(50);  // let the clock settle before config writes (setup only)
  imu.configureSampling();

  sampleQueue = xQueueCreate(SAMPLE_QUEUE_LEN, sizeof(Sample));
  xTaskCreatePinnedToCore(samplerLoop, "imu_sampler", 4096, nullptr,
                          configMAX_PRIORITIES - 1, &samplerTask, 1);

  Serial.printf("# sampling at %lu Hz\n",
                static_cast<unsigned long>(SAMPLE_RATE_HZ));
  Serial.println(SAMPLE_CSV_HEADER);

  sampleTimer = timerBegin(SAMPLE_TIMER_NUM, TIMER_DIVIDER, true);
  timerAttachInterrupt(sampleTimer, &onSampleTimer, true);
  timerAlarmWrite(sampleTimer, TIMER_TICK_HZ / SAMPLE_RATE_HZ, true);
  timerAlarmEnable(sampleTimer);
}

void loop() {
  Sample s;
  if (xQueueReceive(sampleQueue, &s, portMAX_DELAY) == pdTRUE) {
    char line[SAMPLE_CSV_MAX_LEN];
    int n = formatSampleCsv(s, line, sizeof(line));
    Serial.write(reinterpret_cast<const uint8_t *>(line), n);
  }
}
