// CRYPTLINGS firmware for the M5Stack StopWatch (ESP32-S3, 466 x 466 round
// AMOLED). Built by the `stopwatch` PlatformIO environment, which compiles
// the app with -DCRYPT_ROUND=1 and leaves out the C6 board files.
//
// M5Unified does most of the hardware: display, touch, the two side keys,
// speaker, vibration motor, battery, real-time clock and the BMI270 motion
// sensor. The power button lives on the M5PM1 power chip; it is read here
// directly (register 0x48) so a hold can be told from a tap.
//
// Controls:
//   power button   tap: screen on / lights out. Hold 3 s: soft power off (save, then off).
//                  A quick double press is the chip's own hard power off (saved first).
//   side key A     previous tab (wakes the screen when it is dark)
//   side key B     next tab
//   touch          everything else; off while the screen is dark
#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <time.h>

#include <cstring>

#include "app/app.h"
#include "app/gfx.h"
#include "cryptlings_core.h"

using namespace cryptlings;

#ifndef TZ_RULE
#define TZ_RULE "EST5EDT,M3.2.0,M11.1.0"   // America/New_York
#endif

// The IMU's axes in screen terms (x right, y down the screen, z out of the
// glass). Check with the console's `a` command and fix with these flags:
//   lying face up should read z near +1; tipping the right edge down should
//   make x negative; tipping the bottom edge down should make y negative.
#ifndef ACC_X
#define ACC_X (ax)
#endif
#ifndef ACC_Y
#define ACC_Y (ay)
#endif
#ifndef ACC_Z
#define ACC_Z (az)
#endif

// M5PM1 power chip registers (see the M5PM1 library header).
static const uint8_t PM1_IRQ_MASK1 = 0x43, PM1_IRQ_MASK2 = 0x44, PM1_IRQ_MASK3 = 0x45;
static const uint8_t PM1_IRQ_STATUS3 = 0x42;
static const uint8_t PM1_BTN_STATUS = 0x48;   // bit 0: power button held right now
static const uint8_t PM1_BTN_CFG1 = 0x49;     // bit 0: 1 = a single press does NOT reset the board
static const gpio_num_t PIN_PM1_IRQ = GPIO_NUM_12;   // PM1 GPIO1 as IRQ, active low
static const gpio_num_t PIN_KEY_A = GPIO_NUM_2, PIN_KEY_B = GPIO_NUM_1;

static gfx::Color* FB = nullptr;            // 466 x 466 RGB565 in PSRAM (434 KB)
static gfx::Canvas* canvas = nullptr;
static Preferences prefs;
static uint32_t simOffset = 0;
static bool haveTime = false;               // NTP answered this boot
static bool rtcTrusted = false;             // the RTC was set by NTP or by hand, and kept power
static bool ownerSetClock = false;
static int battCache = -2;
static uint32_t battAt = 0;

// ---- time ---------------------------------------------------------------------
static uint32_t nowUnix() { return (uint32_t)time(nullptr) + simOffset; }

static int32_t utcOffsetAt(uint32_t now) {
  time_t t = (time_t)now;
  struct tm lt, gt;
  localtime_r(&t, &lt);
  gmtime_r(&t, &gt);
  gt.tm_isdst = lt.tm_isdst;
  return (int32_t)(t - mktime(&gt));
}

// The RTC (RX8130 at 0x32) keeps UTC. Its flag register 0x1D is
// write-0-to-clear: bit 1 VLF = the time was lost (oscillator stopped or power
// gone), bit 7 VBLF = the backup supply ran low at some point. Only VLF says
// the time is wrong; both are cleared once the time is written.
static const uint8_t RTC_ADDR = 0x32, RTC_FLAGS = 0x1D, RTC_VLF = 0x02, RTC_VBLF = 0x80;
static uint8_t rtcFlags() { return M5.In_I2C.readRegister8(RTC_ADDR, RTC_FLAGS, 400000); }

static void rtcWrite(uint32_t unix) {
  if (!M5.Rtc.isEnabled()) return;
  time_t t = (time_t)unix;
  struct tm g;
  gmtime_r(&t, &g);
  M5.Rtc.setDateTime(&g);
  M5.In_I2C.writeRegister8(RTC_ADDR, RTC_FLAGS, (uint8_t)~(RTC_VLF | RTC_VBLF), 400000);
  prefs.putUChar("rtcOk", 1);
}

static void syncTime() {
  setenv("TZ", TZ_RULE, 1);
  tzset();
  // 1. The real-time clock, if it was ever set and has not lost power.
  bool timeLost = M5.Rtc.isEnabled() && (rtcFlags() & RTC_VLF);
  if (timeLost) prefs.putUChar("rtcOk", 0);    // whatever it holds now is not a real time
  if (M5.Rtc.isEnabled() && !timeLost) {
    m5::rtc_datetime_t dt = M5.Rtc.getDateTime();
    if (dt.date.year >= 2025 && dt.date.year < 2100) {
      struct tm g = {};
      g.tm_year = dt.date.year - 1900; g.tm_mon = dt.date.month - 1; g.tm_mday = dt.date.date;
      g.tm_hour = dt.time.hours; g.tm_min = dt.time.minutes; g.tm_sec = dt.time.seconds;
      setenv("TZ", "UTC0", 1); tzset();
      time_t t = mktime(&g);
      setenv("TZ", TZ_RULE, 1); tzset();
      struct timeval tv = {t, 0};
      settimeofday(&tv, nullptr);
      rtcTrusted = prefs.getUChar("rtcOk", 0) != 0;
    }
  }
#if defined(WIFI_SSID) && defined(WIFI_PASS)
  // 2. NTP, when built with WiFi credentials.
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 6000) delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    configTzTime(TZ_RULE, "pool.ntp.org", "time.nist.gov");
    start = millis();
    while (time(nullptr) < 1700000000 && millis() - start < 6000) delay(100);
    haveTime = time(nullptr) >= 1700000000;
    if (haveTime) { rtcWrite((uint32_t)time(nullptr)); rtcTrusted = true; }
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
#endif
  // 3. Neither: resume from the last saved time (UNVERIFIED).
  if (!haveTime && !rtcTrusted) {
    uint32_t rtc = (uint32_t)time(nullptr);
    uint32_t last = prefs.getUInt("lastEpoch", 1792447200u);
    if (rtc < 1700000000u || rtc < last) {
      struct timeval tv = {(time_t)last, 0};
      settimeofday(&tv, nullptr);
    }
  }
}

// ---- sound and vibration ------------------------------------------------------------
static Note soundQ[32];
static int soundN = 0, soundI = 0;
static uint32_t noteEnd = 0;
static bool speakerOn = false;
static uint32_t vibeUntil = 0;

static void soundService() {
  uint32_t now = millis();
  if (soundI < soundN && (int32_t)(now - noteEnd) >= 0) {
    const Note& n = soundQ[soundI++];
    if (n.hz) {
      if (!speakerOn) { M5.Speaker.begin(); M5.Speaker.setVolume(110); speakerOn = true; }
      M5.Speaker.tone(n.hz, n.ms);
    }
    noteEnd = now + n.ms;
  }
  if (vibeUntil && (int32_t)(now - vibeUntil) >= 0) { M5.Power.setVibration(0); vibeUntil = 0; }
}
static bool soundBusy() { return soundI < soundN || (int32_t)(millis() - noteEnd) < 0 || vibeUntil; }

// The codec and amplifier draw power while started: stop them after 5 s of quiet.
static void speakerIdleOff() {
  static uint32_t quietSince = 0;
  if (!speakerOn) return;
  if (soundBusy()) { quietSince = 0; return; }
  if (!quietSince) quietSince = millis();
  else if (millis() - quietSince > 5000) { M5.Speaker.end(); speakerOn = false; quietSince = 0; }
}

// ---- platform for the app -----------------------------------------------------
struct StopWatchPlatform : Platform {
  uint32_t unixNow() override { return nowUnix(); }
  int32_t utcOffset(uint32_t now) override { return utcOffsetAt(now); }
  bool clockVerified() override { return haveTime || rtcTrusted || ownerSetClock; }
  void setClock(uint32_t unix) override {
    struct timeval tv = {(time_t)(unix - simOffset), 0};
    settimeofday(&tv, nullptr);
    prefs.putUInt("lastEpoch", unix);
    rtcWrite(unix - simOffset);
    ownerSetClock = true;
    rtcTrusted = true;
  }
  void play(const Note* notes, int n) override {
    if (n > 32) n = 32;
    for (int i = 0; i < n; ++i) soundQ[i] = notes[i];
    soundN = n;
    soundI = 0;
    noteEnd = millis();
  }
  void backlight(uint8_t level) override { M5.Display.setBrightness(level); }
  int batteryPercent() override {
    if (battCache == -2) { battCache = M5.Power.getBatteryLevel(); battAt = millis(); }
    return battCache;
  }
  void savePet(const uint8_t* data, size_t len) override {
    prefs.putBytes("pet", data, len);
    prefs.putUInt("lastEpoch", nowUnix());
  }
  void saveSettings(const Settings& s) override { prefs.putBytes("set", &s, sizeof(s)); }
  void vibrate(uint16_t ms) override {
    M5.Power.setVibration(170);
    vibeUntil = millis() + ms;
    if (!vibeUntil) vibeUntil = 1;
  }
  bool accel(float& x, float& y, float& z) override {
    if (!M5.Imu.isEnabled()) return false;
    float ax = 0, ay = 0, az = 0;
    M5.Imu.getAccel(&ax, &ay, &az);   // false only means no new sample yet: the last one stands
    x = ACC_X; y = ACC_Y; z = ACC_Z;
    return true;
  }
};

static StopWatchPlatform platform;
static App* app = nullptr;

// ---- power chip ---------------------------------------------------------------------
static bool pm1Ok = false;
static void pm1Setup() {
  auto& pm = M5.Power.M5pm1;
  pm1Ok = M5.Power.getType() == m5::Power_Class::pmic_t::pmic_m5pm1;
  if (!pm1Ok) return;
  pm.bitOn(PM1_BTN_CFG1, 0x01);                 // a single press must not reset the board
  // Route the power button's click to the ESP (PM1 GPIO1 -> G12) so it can wake
  // light sleep. Same order M5Unified uses on its other PM1 boards: drive the
  // pin high as a push-pull output first, then hand it to the IRQ function.
  using PM = m5::M5PM1_Class;
  pm.setGPIOMode(PM::gpio1, PM::output);
  pm.setGPIODrive(PM::gpio1, PM::push_pull);
  pm.setGPIOPull(PM::gpio1, PM::pull_up);
  pm.setGPIOOutput(PM::gpio1, true);
  pm.setGPIOFunction(PM::gpio1, PM::irq);
  pm.writeRegister8(PM1_IRQ_MASK1, 0x1F);       // no GPIO interrupts
  pm.writeRegister8(PM1_IRQ_MASK2, 0x3F);       // no supply interrupts
  pm.writeRegister8(PM1_IRQ_MASK3, 0x06);       // button: single click only
  pm.clearIRQStatus();
  pm.clearWakeSource();
  pinMode(PIN_PM1_IRQ, INPUT_PULLUP);
}
static bool powerButtonDown() {
  if (!pm1Ok) return false;
  return M5.Power.M5pm1.readRegister8(PM1_BTN_STATUS) & 0x01;
}

// ---- serial console (debug) ----------------------------------------------------
static void printStatus() {
  const State& s = app->pet().state();
  uint32_t now = nowUnix();
  Serial.printf("\n%s  %s  local %.2fh  %s%s  clock %s\n", speciesName((Species)s.species),
                formName((Form)s.form), app->pet().localHour(now), app->pet().awake(now) ? "awake" : "asleep",
                s.hibernating ? " HIBERNATING" : "", haveTime ? "NTP" : rtcTrusted ? "RTC" : "UNVERIFIED");
  Serial.printf("belly %3.0f  cheer %3.0f  rest %3.0f  myst %3.0f  tidy %3.0f  loch %3.0f\n", s.stat[0], s.stat[1],
                s.stat[2], s.stat[3], s.stat[4], s.loch);
  Serial.printf("fork score %.1f -> %s   moon %.2f   battery %d%%\n", app->pet().forkScore(now),
                formName(app->pet().forkPreview(now)), Pet::moonPhase(now), M5.Power.getBatteryLevel());
}

static void console(char c) {
  switch (c) {
    case 's': printStatus(); break;
    case 'w': simOffset += 3600; Serial.println("+1 hour"); break;
    case 'W': simOffset += 86400; Serial.println("+1 day"); break;
    case 'S': app->forceSave(); Serial.println("saved"); break;
    case 'a': {
      float x, y, z;
      if (platform.accel(x, y, z)) Serial.printf("accel (screen terms) x %+.2f  y %+.2f  z %+.2f g\n", x, y, z);
      else Serial.println("no motion sensor");
      break;
    }
    case '?':
      Serial.println(F("CRYPTLINGS console: s status, w +1 hour, W +1 day, S save, a accelerometer. Everything else is on the screen."));
      break;
    default: break;
  }
}

// ---- power ------------------------------------------------------------------------
static const uint32_t HOLD_SHOW_MS = 800;
static const uint32_t HOLD_OFF_MS = 3000;
static bool displayOn = true;

static void softPowerOff() {
  Serial.println(F("Soft power off: saved. Press the power button to turn it back on."));
  app->setPowerHold(0);
  app->renderPowerOff();                    // saves pet + settings, draws the card
  if (!displayOn) { M5.Display.wakeup(); displayOn = true; }
  M5.Display.pushImage(0, 0, gfx::W, gfx::H, FB);
  M5.Display.setBrightness(110);
  delay(1500);
  Serial.flush();
  M5.Display.setBrightness(0);
  M5.Display.sleep();
  M5.Power.setVibration(0);
  if (speakerOn) { M5.Speaker.end(); speakerOn = false; }
  if (pm1Ok) {
    // Give the power button its single-press reset back first: if the chip
    // does not cut power (USB plugged in, a failed write), one press still
    // brings the watch back.
    M5.Power.M5pm1.bitOff(PM1_BTN_CFG1, 0x01);
    for (int i = 0; i < 3; ++i) { M5.Power.M5pm1.powerOff(); delay(200); }
  }
  // Still running: sleep until side key A instead of looking dead.
  esp_sleep_enable_ext0_wakeup(PIN_KEY_A, 0);
  esp_deep_sleep_start();
}

// Light sleep with the screen dark. Returns true when the power button woke
// it. A side key wake returns false: the key is still down, so the next
// M5.update() sees the press and onButton() wakes the screen without also
// changing the tab. The timer wake returns false too.
static bool lightSleep(uint32_t ms) {
  if (speakerOn) { M5.Speaker.end(); speakerOn = false; }
  bool pmArmed = false;
  if (pm1Ok) {
    M5.Power.M5pm1.clearIRQStatus();
    M5.Power.M5pm1.clearWakeSource();
    pmArmed = digitalRead(PIN_PM1_IRQ) == HIGH;   // a stuck-low line would wake it at once
  }
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
  gpio_wakeup_enable(PIN_KEY_A, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(PIN_KEY_B, GPIO_INTR_LOW_LEVEL);
  if (pmArmed) gpio_wakeup_enable(PIN_PM1_IRQ, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
  gpio_wakeup_disable(PIN_KEY_A);
  gpio_wakeup_disable(PIN_KEY_B);
  if (pmArmed) gpio_wakeup_disable(PIN_PM1_IRQ);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  // The power button's click is reported after release, so polling 0x48 in
  // loop() would miss it: read the click flag here, then clear.
  bool power = false;
  if (pm1Ok) {
    power = M5.Power.M5pm1.readRegister8(PM1_IRQ_STATUS3) & 0x01;
    M5.Power.M5pm1.clearIRQStatus();
  }
  return power;
}

// ---- setup / loop -----------------------------------------------------------------
void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  cfg.internal_imu = true;
  cfg.internal_rtc = true;
  cfg.internal_spk = true;
  cfg.internal_mic = false;
  M5.begin(cfg);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) delay(10);

  FB = (gfx::Color*)heap_caps_malloc(gfx::W * gfx::H * sizeof(gfx::Color), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!FB) FB = (gfx::Color*)malloc(gfx::W * gfx::H * sizeof(gfx::Color));
  canvas = new gfx::Canvas(FB);
  app = new App(platform, *canvas);

  M5.Display.setRotation(0);
#ifndef FB_SWAP_BYTES
#define FB_SWAP_BYTES 1                     // our framebuffer is native-endian RGB565
#endif
  M5.Display.setSwapBytes(FB_SWAP_BYTES);
  canvas->fill(gfx::hex(0x0B0D12));
  M5.Display.pushImage(0, 0, gfx::W, gfx::H, FB);
  M5.Display.setBrightness(150);
  pm1Setup();

  prefs.begin("cryptlings", false);
  syncTime();

  uint8_t petBuf[sizeof(State)];
  size_t petLen = prefs.isKey("pet") ? prefs.getBytes("pet", petBuf, sizeof(petBuf)) : 0;
  Settings set{};
  std::memset(&set, 0, sizeof(set));
  bool haveSet = false;
  if (prefs.isKey("set")) {
    size_t len = prefs.getBytesLength("set");
    if (len > 0 && len <= sizeof(set)) {
      prefs.getBytes("set", &set, len);
      haveSet = set.magic == SETTINGS_MAGIC;
    }
  }
  set.name[sizeof(set.name) - 1] = 0;
  app->begin(millis(), petLen ? petBuf : nullptr, petLen, haveSet ? &set : nullptr);
  Serial.printf("CRYPTLINGS (StopWatch) up. %dx%d, PSRAM %s, power chip %s, IMU %s, RTC %s, clock %s, free heap %u\n",
                M5.Display.width(), M5.Display.height(), psramFound() ? "yes" : "NO", pm1Ok ? "ok" : "NOT FOUND",
                M5.Imu.isEnabled() ? "ok" : "none", M5.Rtc.isEnabled() ? "ok" : "none",
                haveTime ? "NTP" : rtcTrusted ? "RTC" : "UNVERIFIED", (unsigned)ESP.getFreeHeap());
}

void loop() {
  M5.update();
  while (Serial.available()) console((char)Serial.read());
  uint32_t now = millis();
  TouchIn t{false, 0, 0};

  // ---- power button: tap = screen on / lights out, hold = soft power off --------------
  static bool pwrWas = false, pwrWoke = false, pwrIgnore = true;
  static uint32_t pwrDownAt = 0, pwrUpAt = 0;
  bool pwr = powerButtonDown();
  if (pwrIgnore && !pwr) pwrIgnore = false;     // still held from the press that powered it on
  if (pwr && !pwrWas && !pwrIgnore) {
    pwrDownAt = now;
    pwrWoke = !app->screenOn();
    app->forceSave();                           // a quick double press is a hard power off: save first
    if (pwrWoke) app->onButton(0);              // when dark, any key only wakes
  }
  if (pwr && !pwrIgnore) {
    uint32_t held = now - pwrDownAt;
    if (held > HOLD_SHOW_MS) app->setPowerHold((float)(held - HOLD_SHOW_MS) / (HOLD_OFF_MS - HOLD_SHOW_MS));
    if (held >= HOLD_OFF_MS) softPowerOff();
  }
  if (!pwr && pwrWas && !pwrIgnore) {
    if (!pwrWoke && now - pwrDownAt < HOLD_SHOW_MS) app->sleepNow();
    app->setPowerHold(0);
    pwrUpAt = now;
  }
  pwrWas = pwr;
  // A tap shorter than one frame can fall between two polls of 0x48. The power
  // chip still reports the click (M5.update() reads it into BtnPWR): use it.
  if (M5.BtnPWR.wasClicked() && !pwr && now - pwrUpAt > 1000 && now - pwrDownAt > 1000) {
    app->forceSave();
    if (app->screenOn()) app->sleepNow();
    else app->onButton(0);
  }

  // ---- side keys --------------------------------------------------------------------
  if (M5.BtnA.wasPressed()) app->onButton(0);
  if (M5.BtnB.wasPressed()) app->onButton(1);

  // ---- touch: only while the screen is on ----------------------------------------------
  if (app->screenOn()) {
    auto d = M5.Touch.getDetail();
    if (d.isPressed()) { t.down = true; t.x = d.x; t.y = d.y; }
  }

  // ---- app + display ------------------------------------------------------------------
  if (app->update(now, t)) {
    if (!displayOn) { M5.Display.wakeup(); displayOn = true; }
    app->render();
    M5.Display.pushImage(0, 0, gfx::W, gfx::H, FB);
  }
  if (!app->screenOn() && displayOn) { M5.Display.sleep(); displayOn = false; }
  soundService();
  speakerIdleOff();

  // ---- battery: once a minute with the screen on, never while asleep -------------------
  if (app->screenOn() && (battCache == -2 || now - battAt > 60000UL)) {
    battCache = M5.Power.getBatteryLevel();
    battAt = now;
    static uint8_t empty = 0;
    bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging_t::is_charging;
    if (battCache == 0 && !charging) { if (++empty >= 2) softPowerOff(); }
    else empty = 0;
  }

  // ---- rest -----------------------------------------------------------------------------
  if (app->screenOn() || pwr) {
    delay(5);
  } else if (app->settings().saver && !soundBusy() && !Serial) {
    // Wakes on a key, the power button, or once a minute to keep the pet ticking.
    if (lightSleep(60000)) app->onButton(0);    // a press only wakes the screen
  } else {
    delay(100);
  }
}
