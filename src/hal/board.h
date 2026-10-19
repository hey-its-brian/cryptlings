// Waveshare ESP32-C6-Touch-LCD-1.47 hardware layer.
// Pins, the JD9853 init table and the AXS5106L protocol come from Waveshare's
// official demo code and schematic (see docs/HARDWARE.md for sources).
#pragma once
#ifdef ARDUINO
#include <Arduino.h>

namespace board {

// ---- pins (official schematic) ----
static const int PIN_LCD_SCLK = 1, PIN_LCD_MOSI = 2, PIN_LCD_CS = 14, PIN_LCD_DC = 15, PIN_LCD_RST = 22;
static const int PIN_BACKLIGHT = 23;        // active high, PWM
static const int PIN_SD_MISO = 3, PIN_SD_CS = 4;  // SD shares SCLK/MOSI; keep its CS high
static const int PIN_I2C_SDA = 18, PIN_I2C_SCL = 19, PIN_TP_RST = 20, PIN_TP_INT = 21;
static const int PIN_BAT_ADC = 0;           // battery through a 200K/100K divider (x3)
static const int PIN_BOOT = 9;              // BOOT button, active low
#ifndef BUZZER_PIN
#define BUZZER_PIN 17                       // external passive buzzer on header pin 7 (ESP_RXD) (-1 = none)
#endif
#ifndef WAKE_BUTTON_PIN
#define WAKE_BUTTON_PIN 7                   // external momentary button to GND, header pin 22 (-1 = none)
#endif                                      // must be GPIO0..7: only those can wake the chip from soft power off
#ifndef LCD_SPI_HZ
#define LCD_SPI_HZ 40000000                 // Waveshare's IDF demo runs 80 MHz; 40 MHz is the safe start
#endif

static const int LCD_W = 172, LCD_H = 320, LCD_X_OFFSET = 34;
static const uint8_t TOUCH_ADDR = 0x63;
static const uint8_t IMU_ADDR = 0x6B;

void releaseHolds();                        // first thing in setup (pins held through soft power off)
void displayBegin();
void displayPush(const uint16_t* fb);       // full 172 x 320 native-endian RGB565 frame
void displaySleep();                        // panel off + sleep-in (a few uA)
void displayWake();                         // sleep-out + panel on (about 120 ms)
void backlight(uint8_t level);              // 0 off .. 255 full

bool touchBegin();                          // false if the controller did not answer
bool touchRead(int& x, int& y);             // true while touched, portrait coordinates
void touchSleep();                          // hold the touch chip in reset (fully off)
void touchWake();                           // release reset, ready again in about 50 ms
void imuOff();                              // the QMI8658 is unused: make sure it is powered down

void buzzerBegin();
void buzzerQueue(const uint16_t* hz, const uint16_t* ms, int n);
void buzzerService();                       // call every loop
bool buzzerBusy();

int batteryPercent();                       // -1 when no battery is detected
bool bootPressed();
bool wakeButtonPressed();                   // false when no wake button is configured

// Light sleep until the wake button, BOOT, or the timer (RAM and screen contents kept).
void lightSleep(uint32_t ms);
// Soft power off: everything saved by the caller, then deep sleep until the wake
// button is pressed. The chip reboots on wake; the clock keeps running.
void powerOff();

}  // namespace board
#endif
