#include <ESP32Servo.h>

Servo patServo;

const int SERVO_PIN = 13;

// range aman MG90S
const int MIN_US = 600;
const int MAX_US = 2400;

void setup() {
  Serial.begin(115200);
  patServo.attach(SERVO_PIN, 500, 2500);
  patServo.writeMicroseconds(1000); // posisi awal
}

void smoothMove(int fromUs, int toUs, int stepDelay) {
  if (fromUs < toUs) {
    for (int us = fromUs; us <= toUs; us += 10) {
      patServo.writeMicroseconds(us);
      delay(stepDelay);
    }
  } else {
    for (int us = fromUs; us >= toUs; us -= 10) {
      patServo.writeMicroseconds(us);
      delay(stepDelay);
    }
  }
}

void patPat(int times) {
  for (int i = 0; i < times; i++) {
    smoothMove(900, 2000, 5);   // MAJU
    delay(80);
    smoothMove(2000, 900, 5);  // BALIK
    delay(150);
  }
}

void loop() {
  patPat(2);
  delay(2000);
}
