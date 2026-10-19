#ifdef ARDUINO
#include "board.h"

#include <SPI.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

namespace board {

// ---- display: JD9853 over SPI ---------------------------------------------
namespace {
struct Cmd { uint8_t cmd; uint8_t len; uint16_t delayMs; uint8_t data[32]; };

// Waveshare's vendor init table (IDF esp_lcd_jd9853 + bsp_display_init), RGB565,
// portrait, RGB order, inversion on.
const Cmd INIT[] = {
  {0x11, 0, 120, {0}},
  {0xDF, 2, 0, {0x98, 0x53}},
  {0xDF, 2, 0, {0x98, 0x53}},
  {0xB2, 1, 0, {0x23}},
  {0xB7, 4, 0, {0x00, 0x47, 0x00, 0x6F}},
  {0xBB, 6, 0, {0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0}},
  {0xC0, 2, 0, {0x44, 0xA4}},
  {0xC1, 1, 0, {0x16}},
  {0xC3, 8, 0, {0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77}},
  {0xC4, 12, 0, {0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82}},
  {0xC8, 32, 0, {0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
                 0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00}},
  {0xD0, 5, 0, {0x04, 0x06, 0x6B, 0x0F, 0x00}},
  {0xD7, 2, 0, {0x00, 0x30}},
  {0xE6, 1, 0, {0x14}},
  {0xDE, 1, 0, {0x01}},
  {0xB7, 5, 0, {0x03, 0x13, 0xEF, 0x35, 0x35}},
  {0xC1, 3, 0, {0x14, 0x15, 0xC0}},
  {0xC2, 2, 0, {0x06, 0x3A}},
  {0xC4, 2, 0, {0x72, 0x12}},
  {0xBE, 1, 0, {0x00}},
  {0xDE, 1, 0, {0x02}},
  {0xE5, 3, 0, {0x00, 0x02, 0x00}},
  {0xE5, 3, 0, {0x01, 0x02, 0x00}},
  {0xDE, 1, 0, {0x00}},
  {0x35, 1, 0, {0x00}},
  {0x3A, 1, 0, {0x05}},
  {0x2A, 4, 0, {0x00, 0x22, 0x00, 0xCD}},
  {0x2B, 4, 0, {0x00, 0x00, 0x01, 0x3F}},
  {0xDE, 1, 0, {0x02}},
  {0xE5, 3, 0, {0x00, 0x02, 0x00}},
  {0xDE, 1, 0, {0x00}},
  {0x36, 1, 0, {0x00}},
  {0x21, 0, 10, {0}},
  {0x29, 0, 0, {0}},
};

SPISettings lcdSpi(LCD_SPI_HZ, MSBFIRST, SPI_MODE0);
uint8_t chunk[4096];

void lcdCmd(uint8_t c, const uint8_t* d, int n) {
  digitalWrite(PIN_LCD_CS, LOW);
  digitalWrite(PIN_LCD_DC, LOW);
  SPI.write(c);
  if (n) {
    digitalWrite(PIN_LCD_DC, HIGH);
    SPI.writeBytes(d, n);
  }
  digitalWrite(PIN_LCD_CS, HIGH);
}
}  // namespace

void releaseHolds() {
  gpio_hold_dis((gpio_num_t)PIN_BACKLIGHT);
  gpio_hold_dis((gpio_num_t)PIN_TP_RST);
#if WAKE_BUTTON_PIN >= 0
  rtc_gpio_deinit((gpio_num_t)WAKE_BUTTON_PIN);
  pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);
#endif
}

void displaySleep() {
  SPI.beginTransaction(lcdSpi);
  lcdCmd(0x28, nullptr, 0);                 // DISPOFF
  lcdCmd(0x10, nullptr, 0);                 // SLPIN
  SPI.endTransaction();
  delay(5);
}

void displayWake() {
  SPI.beginTransaction(lcdSpi);
  lcdCmd(0x11, nullptr, 0);                 // SLPOUT
  SPI.endTransaction();
  delay(120);
  SPI.beginTransaction(lcdSpi);
  lcdCmd(0x29, nullptr, 0);                 // DISPON
  SPI.endTransaction();
}

void displayBegin() {
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);           // keep the SD card off the shared bus
  pinMode(PIN_LCD_CS, OUTPUT);
  pinMode(PIN_LCD_DC, OUTPUT);
  pinMode(PIN_LCD_RST, OUTPUT);
  digitalWrite(PIN_LCD_CS, HIGH);
  SPI.begin(PIN_LCD_SCLK, PIN_SD_MISO, PIN_LCD_MOSI, -1);

  digitalWrite(PIN_LCD_RST, HIGH); delay(20);
  digitalWrite(PIN_LCD_RST, LOW);  delay(20);
  digitalWrite(PIN_LCD_RST, HIGH); delay(120);

  SPI.beginTransaction(lcdSpi);
  for (const Cmd& c : INIT) {
    lcdCmd(c.cmd, c.data, c.len);
    if (c.delayMs) delay(c.delayMs);
  }
  SPI.endTransaction();

  // Explicit channels keep the backlight and buzzer on different LEDC timers
  // on every 3.x core (older 3.0 to 3.2 cores paired channels 0 and 1).
  ledcAttachChannel(PIN_BACKLIGHT, 5000, 10, 0);
  backlight(0);
}

void displayPush(const uint16_t* fb) {
  const uint8_t caset[4] = {0, (uint8_t)LCD_X_OFFSET, 0, (uint8_t)(LCD_X_OFFSET + LCD_W - 1)};
  const uint8_t raset[4] = {0, 0, (uint8_t)((LCD_H - 1) >> 8), (uint8_t)((LCD_H - 1) & 0xFF)};
  SPI.beginTransaction(lcdSpi);
  lcdCmd(0x2A, caset, 4);
  lcdCmd(0x2B, raset, 4);
  digitalWrite(PIN_LCD_CS, LOW);
  digitalWrite(PIN_LCD_DC, LOW);
  SPI.write(0x2C);
  digitalWrite(PIN_LCD_DC, HIGH);
  const int total = LCD_W * LCD_H;
  const int per = sizeof(chunk) / 2;
  for (int i = 0; i < total; i += per) {
    int n = total - i < per ? total - i : per;
    for (int k = 0; k < n; ++k) {           // the panel wants big-endian RGB565
      uint16_t c = fb[i + k];
      chunk[2 * k] = c >> 8;
      chunk[2 * k + 1] = c & 0xFF;
    }
    SPI.writeBytes(chunk, n * 2);
  }
  digitalWrite(PIN_LCD_CS, HIGH);
  SPI.endTransaction();
}

void backlight(uint8_t level) { ledcWrite(PIN_BACKLIGHT, (uint32_t)level * 1023 / 255); }

// ---- touch: AXS5106L over I2C ---------------------------------------------
static bool touchOk = false;

bool touchBegin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  pinMode(PIN_TP_INT, INPUT_PULLUP);
  pinMode(PIN_TP_RST, OUTPUT);
  digitalWrite(PIN_TP_RST, LOW);  delay(200);
  digitalWrite(PIN_TP_RST, HIGH); delay(300);
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0x08);                         // ID register
  touchOk = Wire.endTransmission() == 0;
  if (touchOk) {
    Wire.requestFrom((int)TOUCH_ADDR, 3);
    while (Wire.available()) Wire.read();
  }
  return touchOk;
}

void touchSleep() {
  digitalWrite(PIN_TP_RST, LOW);
  touchOk = false;
}

void touchWake() {
  digitalWrite(PIN_TP_RST, HIGH);
  delay(50);
  Wire.beginTransmission(TOUCH_ADDR);
  touchOk = Wire.endTransmission() == 0;
}

void imuOff() {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x08);                         // CTRL7: accelerometer and gyro disabled
  Wire.write(0x00);
  Wire.endTransmission();
}

bool touchRead(int& x, int& y) {
  if (!touchOk) return false;
  uint8_t d[14] = {0};
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((int)TOUCH_ADDR, 14) != 14) return false;
  for (int i = 0; i < 14; ++i) d[i] = Wire.read();
  int n = d[1] & 0x0F;
  if (n == 0 || n > 2) return false;
  int rx = ((d[2] & 0x0F) << 8) | d[3];
  int ry = ((d[4] & 0x0F) << 8) | d[5];
  x = (LCD_W - 1) - rx;                     // raw X is mirrored in portrait
  y = ry;
  if (x < 0) x = 0;
  if (x >= LCD_W) x = LCD_W - 1;
  if (y < 0) y = 0;
  if (y >= LCD_H) y = LCD_H - 1;
  return true;
}

// ---- buzzer (external, passive) -------------------------------------------
namespace {
uint16_t qHz[24], qMs[24];
int qHead = 0, qLen = 0;
uint32_t noteEnd = 0;
bool playing = false;
}  // namespace

void buzzerBegin() {
#if BUZZER_PIN >= 0
  ledcAttachChannel(BUZZER_PIN, 2000, 10, 2);
  ledcWrite(BUZZER_PIN, 0);
#endif
}

void buzzerQueue(const uint16_t* hz, const uint16_t* ms, int n) {
  for (int i = 0; i < n && qLen < 24; ++i) {
    int k = (qHead + qLen) % 24;
    qHz[k] = hz[i];
    qMs[k] = ms[i];
    ++qLen;
  }
}

void buzzerService() {
#if BUZZER_PIN >= 0
  uint32_t now = millis();
  if (playing && (int32_t)(now - noteEnd) < 0) return;
  if (qLen == 0) {
    if (playing) { ledcWrite(BUZZER_PIN, 0); playing = false; }
    return;
  }
  uint16_t hz = qHz[qHead], ms = qMs[qHead];
  qHead = (qHead + 1) % 24;
  --qLen;
  if (hz) {
    ledcChangeFrequency(BUZZER_PIN, hz, 10);
    ledcWrite(BUZZER_PIN, 200);             // about 20% duty: a softer tone than 50%
  } else {
    ledcWrite(BUZZER_PIN, 0);
  }
  noteEnd = now + ms;
  playing = true;
#endif
}

bool buzzerBusy() { return playing || qLen > 0; }

// ---- battery and button ------------------------------------------------------
int batteryPercent() {
  uint32_t mv = analogReadMilliVolts(PIN_BAT_ADC) * 3;
  if (mv < 2500) return -1;                 // nothing on the battery pins
  if (mv >= 4150) return 100;
  if (mv <= 3300) return 0;
  return (int)((mv - 3300) * 100 / 850);
}

bool bootPressed() { return digitalRead(PIN_BOOT) == LOW; }

bool wakeButtonPressed() {
#if WAKE_BUTTON_PIN >= 0
  return digitalRead(WAKE_BUTTON_PIN) == LOW;
#else
  return false;
#endif
}

// ---- sleep -------------------------------------------------------------------
void lightSleep(uint32_t ms) {
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
#if WAKE_BUTTON_PIN >= 0
  gpio_wakeup_enable((gpio_num_t)WAKE_BUTTON_PIN, GPIO_INTR_LOW_LEVEL);
#endif
  gpio_wakeup_enable((gpio_num_t)PIN_BOOT, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
#if WAKE_BUTTON_PIN >= 0
  gpio_wakeup_disable((gpio_num_t)WAKE_BUTTON_PIN);
#endif
  gpio_wakeup_disable((gpio_num_t)PIN_BOOT);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
}

void powerOff() {
  backlight(0);
  displaySleep();
  touchSleep();
  imuOff();
#if BUZZER_PIN >= 0
  ledcWrite(BUZZER_PIN, 0);
#endif
  // Hold the backlight transistor and the touch reset low through deep sleep.
  ledcDetach(PIN_BACKLIGHT);
  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, LOW);
  gpio_hold_en((gpio_num_t)PIN_BACKLIGHT);
  gpio_hold_en((gpio_num_t)PIN_TP_RST);
  // (the C6 keeps single-pin holds through deep sleep, no global hold needed)
#if WAKE_BUTTON_PIN >= 0
  // Wait for the hand to let go, or the button would wake it straight back up.
  uint32_t t0 = millis();
  while (wakeButtonPressed() && millis() - t0 < 10000) delay(10);
  delay(50);
  rtc_gpio_pullup_en((gpio_num_t)WAKE_BUTTON_PIN);
  rtc_gpio_pulldown_dis((gpio_num_t)WAKE_BUTTON_PIN);
  esp_sleep_enable_ext1_wakeup_io(1ULL << WAKE_BUTTON_PIN, ESP_EXT1_WAKEUP_ANY_LOW);
#endif
  esp_deep_sleep_start();
}

}  // namespace board
#endif
