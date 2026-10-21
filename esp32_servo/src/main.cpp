#include <Arduino.h>
#include <driver/gpio.h>

static constexpr uint8_t SERVO_PIN = 21;
static constexpr float SERVO_MIN_ANGLE = -90.0f;
static constexpr float SERVO_MAX_ANGLE = 90.0f;
static constexpr uint16_t SERVO_MIN_US = 1000;
static constexpr uint16_t SERVO_CENTER_US = 1500;
static constexpr uint16_t SERVO_MAX_US = 2000;
static constexpr uint16_t SERVO_STEP_DELAY_MS = 80;

static float currentAngle = 0.0f;
static volatile uint16_t servoPulseUs = SERVO_CENTER_US;

static void servoTask(void *) {
  TickType_t next = xTaskGetTickCount();
  for (;;) {
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(servoPulseUs);
    digitalWrite(SERVO_PIN, LOW);
    vTaskDelayUntil(&next, pdMS_TO_TICKS(20));
  }
}

static void writeAngle(float angle) {
  angle = constrain(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);

  // Quy ước góc cơ khí: -90° = 1 ms, 0° = 1.5 ms, +90° = 2 ms.
  // Nội suy riêng hai nửa giúp giữ đúng tâm khi hiệu chỉnh min/max về sau.
  float pulse;
  if (angle < 0.0f) {
    pulse = SERVO_CENTER_US +
            (SERVO_CENTER_US - SERVO_MIN_US) * angle / 90.0f;
  } else {
    pulse = SERVO_CENTER_US +
            (SERVO_MAX_US - SERVO_CENTER_US) * angle / 90.0f;
  }
  servoPulseUs = (uint16_t)lroundf(pulse);
  currentAngle = angle;
}

static void moveSmooth(float target) {
  target = constrain(target, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
  float step = target >= currentAngle ? 1.0f : -1.0f;
  while (fabsf(target - currentAngle) >= 1.0f) {
    writeAngle(currentAngle + step);
    delay(SERVO_STEP_DELAY_MS);
  }
  writeAngle(target);
  delay(30);
  unsigned long measuredUs = pulseIn(SERVO_PIN, HIGH, 30000UL);
  Serial.printf("ANGLE %.0f PWM %lu us\n", currentAngle, measuredUs);
}

void setup() {
  Serial.begin(115200);
  pinMode(SERVO_PIN, OUTPUT);
  gpio_set_direction((gpio_num_t)SERVO_PIN, GPIO_MODE_INPUT_OUTPUT);
  writeAngle(0.0f);
  xTaskCreatePinnedToCore(servoTask, "servo", 2048, nullptr, 2, nullptr, 0);
  delay(500);
  Serial.println("ESP32-S3 SERVO READY GPIO21; enter -90..90 (0=center)");
}

void loop() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;
  char *end = nullptr;
  float target = strtof(line.c_str(), &end);
  if (end == line.c_str() || *end != '\0' ||
      target < SERVO_MIN_ANGLE || target > SERVO_MAX_ANGLE) {
    Serial.println("ERR enter angle -90..90 (0=center)");
    return;
  }
  moveSmooth(target);
}
