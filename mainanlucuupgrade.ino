#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "driver/i2s.h"
#include <ESP32Servo.h>

/* ================= PIN ================= */
#define BUZZER_PIN 25
#define SERVO_PIN  13

#define SDA_PIN 21
#define SCL_PIN 22

#define I2S_WS   27
#define I2S_SD   33
#define I2S_SCK  26

/* ================= OLED ================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* ================= SERVO ================= */
Servo patServo;
const int SERVO_MIN = 900;
const int SERVO_MAX = 1800;

/* ================= AUDIO MIC ================= */
#define I2S_PORT I2S_NUM_0
#define SAMPLE_BUFFER_SIZE 256
int32_t samples[SAMPLE_BUFFER_SIZE];

/* ====== SOUND TUNING ====== */
#define THRESHOLD_HELLO   30
#define THRESHOLD_PUKPUK  40
#define LONG_SOUND_MS    400
#define VERY_LONG_MS     600

bool soundActive = false;
unsigned long soundStart = 0;
long peakAmplitude = 0;

/* ================= STATE ================= */
enum FACE { NORMAL, HAPPY, SHY };
FACE faceState = NORMAL;

bool pukpukActive = false;
unsigned long pukpukStart = 0;

/* ===== SERVO NON-BLOCKING ===== */
unsigned long lastPat = 0;
bool patDir = false;

/* ===== EYE ANIMATION ===== */
int baseLeftEyeX  = 34;
int baseRightEyeX = 72;
int baseEyeY      = 18;
int eyeWidth      = 22;
int eyeHeight     = 28;

int targetOffsetX = 0, targetOffsetY = 0;
int offsetX = 0, offsetY = 0;
int moveSpeed = 6;

unsigned long lastMoveTime = 0;
unsigned long lastBlinkTime = 0;
bool blink = false;

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  patServo.attach(SERVO_PIN, 500, 2500);
  patServo.writeMicroseconds(1100);

  setupMic();

  display.clearDisplay();
  display.display();

  Serial.println("SYSTEM READY");
}

/* ================= LOOP ================= */
void loop() {
  listenSound();
  updatePukPuk();
  drawFace();
}

/* ================= MIC ================= */
void setupMic() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pins);
}

/* ================= SOUND LOGIC ================= */
void listenSound() {
  size_t bytesRead;
  i2s_read(I2S_PORT, samples, sizeof(samples), &bytesRead, portMAX_DELAY);

  long sum = 0;
  for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
    sum += abs(samples[i] >> 16);
  }
  long amplitude = sum / SAMPLE_BUFFER_SIZE;

  Serial.println(amplitude);

  unsigned long now = millis();

  if (amplitude > THRESHOLD_HELLO) {
    if (!soundActive) {
      soundActive = true;
      soundStart = now;
      peakAmplitude = amplitude;
    }
    if (amplitude > peakAmplitude) peakAmplitude = amplitude;
  } 
  else if (soundActive) {
    unsigned long duration = now - soundStart;
    soundActive = false;

    if (peakAmplitude > THRESHOLD_PUKPUK && duration > VERY_LONG_MS) {
      startPukPuk();
    } 
    else if (duration > LONG_SOUND_MS) {
      doHello();
    }

    peakAmplitude = 0;
  }
}

/* ================= ACTION ================= */
void doHello() {
  faceState = HAPPY;

  tone(BUZZER_PIN, 900, 120);
  delay(150);
  tone(BUZZER_PIN, 1200, 120);
  delay(150);
  tone(BUZZER_PIN, 1500, 150);
}

/* ================= PUKPUK ================= */
void startPukPuk() {
  if (pukpukActive) return;

  faceState = SHY;
  pukpukActive = true;
  pukpukStart = millis();
}

void updatePukPuk() {
  if (!pukpukActive) return;

  if (millis() - pukpukStart > 20000) {
    pukpukActive = false;
    faceState = NORMAL;
    patServo.writeMicroseconds(1100);
    return;
  }

  if (millis() - lastPat > 300) { // kecepatan pukpuk
    patDir = !patDir;
    patServo.writeMicroseconds(patDir ? SERVO_MAX : SERVO_MIN);
    lastPat = millis();
  }
}

/* ================= OLED FACE ================= */
void drawFace() {
  unsigned long now = millis();

  if (!blink && now - lastBlinkTime > random(2500, 4500)) {
    blink = true;
    lastBlinkTime = now;
  }
  if (blink && now - lastBlinkTime > 120) {
    blink = false;
    lastBlinkTime = now;
  }

  if (!blink && now - lastMoveTime > random(1200, 2800)) {
    int m = random(0, 7);
    targetOffsetX = (m % 3 - 1) * 8;
    targetOffsetY = (m / 3 - 1) * 6;
    lastMoveTime = now;
  }

  offsetX += (targetOffsetX - offsetX) / moveSpeed;
  offsetY += (targetOffsetY - offsetY) / moveSpeed;

  display.clearDisplay();

  int eH = blink ? 4 : eyeHeight;
  int eY = baseEyeY + offsetY + (blink ? eyeHeight / 2 : 0);

  if (faceState == SHY) {
    display.fillRoundRect(baseLeftEyeX + offsetX, 30, 18, 6, 3, WHITE);
    display.fillRoundRect(baseRightEyeX + offsetX, 30, 18, 6, 3, WHITE);
  } else {
    display.fillRoundRect(baseLeftEyeX + offsetX, eY, eyeWidth, eH, 5, WHITE);
    display.fillRoundRect(baseRightEyeX + offsetX, eY, eyeWidth, eH, 5, WHITE);
  }

  if (faceState == HAPPY) {
    display.drawLine(52, 48, 76, 48, WHITE);
    display.drawLine(54, 49, 74, 49, WHITE);
    display.drawLine(56, 50, 72, 50, WHITE);
  } 
  else if (faceState == SHY) {
    display.drawLine(58, 50, 70, 50, WHITE);
    display.fillCircle(30, 46, 2, WHITE);
    display.fillCircle(96, 46, 2, WHITE);
  } 
  else {
    display.drawLine(58, 50, 70, 50, WHITE);
  }

  display.display();
}
