// CRYPTLINGS firmware: the full touch UI on the Waveshare ESP32-C6-Touch-LCD-1.47.
//
// The app (src/app) is portable and also runs in the desktop simulator; this
// file wires it to the board: display, touch, buzzer, backlight, battery,
// NVS storage and the clock. A small serial console stays for debugging.
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>

#include <cstring>

#include "app/app.h"
#include "app/gfx.h"
#include "cryptlings_core.h"
#include "hal/board.h"

using namespace cryptlings;

#ifndef TZ_RULE
#define TZ_RULE "EST5EDT,M3.2.0,M11.1.0"   // America/New_York
#endif

static gfx::Color FB[gfx::W * gfx::H];      // 110 KB frame buffer, pushed whole
static gfx::Canvas canvas(FB);
static Preferences prefs;
static uint32_t simOffset = 0;              // console fast-forward ('w' / 'W')
static bool haveTime = false;
static bool ownerSetClock = false;          // set by hand this power-up (MORE tab)
static int battCache = -2;                  // battery %: read once a minute while awake only
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

static void syncTime() {
  setenv("TZ", TZ_RULE, 1);
  tzset();
#if defined(WIFI_SSID) && defined(WIFI_PASS)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 6000) delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    configTzTime(TZ_RULE, "pool.ntp.org", "time.nist.gov");
    start = millis();
    while (time(nullptr) < 1700000000 && millis() - start < 6000) delay(100);
    haveTime = time(nullptr) >= 1700000000;   // verified only when NTP answered
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
#endif
  if (!haveTime) {
    // No NTP: the clock is UNVERIFIED. The RTC timer survives a soft reset (but
    // not a power cycle), so keep it when plausible, else resume from the last
    // saved time. The player can also set it on the MORE tab.
    uint32_t rtc = (uint32_t)time(nullptr);
    uint32_t last = prefs.getUInt("lastEpoch", 1792447200u);
    if (rtc < 1700000000u || rtc < last) {
      struct timeval tv = {(time_t)last, 0};
      settimeofday(&tv, nullptr);
    }
  }
}

// ---- platform for the app -----------------------------------------------------
struct BoardPlatform : Platform {
  uint32_t unixNow() override { return nowUnix(); }
  int32_t utcOffset(uint32_t now) override { return utcOffsetAt(now); }
  bool clockVerified() override { return haveTime || ownerSetClock; }
  void setClock(uint32_t unix) override {
    struct timeval tv = {(time_t)(unix - simOffset), 0};
    settimeofday(&tv, nullptr);
    prefs.putUInt("lastEpoch", unix);
    ownerSetClock = true;
  }
  void play(const Note* notes, int n) override {
    uint16_t hz[16], ms[16];
    if (n > 16) n = 16;
    for (int i = 0; i < n; ++i) { hz[i] = notes[i].hz; ms[i] = notes[i].ms; }
    board::buzzerQueue(hz, ms, n);
  }
  void backlight(uint8_t level) override { board::backlight(level); }
  int batteryPercent() override {
    if (battCache == -2) { battCache = board::batteryPercent(); battAt = millis(); }
    return battCache;
  }
  void savePet(const uint8_t* data, size_t len) override {
    prefs.putBytes("pet", data, len);
    prefs.putUInt("lastEpoch", nowUnix());
  }
  void saveSettings(const Settings& s) override { prefs.putBytes("set", &s, sizeof(s)); }
};

static BoardPlatform platform;
static App app(platform, canvas);

// ---- serial console (debug) ----------------------------------------------------
static void printStatus() {
  const State& s = app.pet().state();
  uint32_t now = nowUnix();
  Serial.printf("\n%s  %s  local %.2fh  %s%s  clock %s\n", speciesName((Species)s.species),
                formName((Form)s.form), app.pet().localHour(now), app.pet().awake(now) ? "awake" : "asleep",
                s.hibernating ? " HIBERNATING" : "", haveTime ? "NTP" : "UNVERIFIED");
  Serial.printf("belly %3.0f  cheer %3.0f  rest %3.0f  myst %3.0f  tidy %3.0f  loch %3.0f\n", s.stat[0], s.stat[1],
                s.stat[2], s.stat[3], s.stat[4], s.loch);
  Serial.printf("fork score %.1f -> %s   moon %.2f   battery %d%%\n", app.pet().forkScore(now),
                formName(app.pet().forkPreview(now)), Pet::moonPhase(now), board::batteryPercent());
}

static void console(char c) {
  switch (c) {
    case 's': printStatus(); break;
    case 'w': simOffset += 3600; Serial.println("+1 hour"); break;
    case 'W': simOffset += 86400; Serial.println("+1 day"); break;
    case 'S': app.forceSave(); Serial.println("saved"); break;
    case '?':
      Serial.println(F("CRYPTLINGS console: s status, w +1 hour, W +1 day, S save. Everything else is on the touch screen."));
      break;
    default: break;
  }
}

// ---- setup / loop -----------------------------------------------------------------
// ---- power button ---------------------------------------------------------------
static const uint32_t HOLD_SHOW_MS = 800;   // after this, the "keep holding" card appears
static const uint32_t HOLD_OFF_MS = 3000;   // soft power off
static bool displayOn = true;
static bool touchOn = true;

static void softPowerOff() {
  Serial.println(F("Soft power off: saved. Press the button to wake."));
  app.setPowerHold(0);
  app.renderPowerOff();                     // saves pet + settings, draws the card
  if (!displayOn) { board::displayWake(); displayOn = true; }
  board::displayPush(FB);
  board::backlight(120);
  delay(1500);
  Serial.flush();
  board::powerOff();                        // does not return
}

void setup() {
  board::releaseHolds();
  Serial.setTxBufferSize(1024);
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) delay(10);

  pinMode(board::PIN_BOOT, INPUT_PULLUP);
  board::displayBegin();
  canvas.fill(gfx::hex(0x0B0D12));
  board::displayPush(FB);
  board::backlight(150);
  bool touch = board::touchBegin();
  board::imuOff();
  board::buzzerBegin();

  prefs.begin("cryptlings", false);
  syncTime();

  uint8_t petBuf[sizeof(State)];
  size_t petLen = prefs.isKey("pet") ? prefs.getBytes("pet", petBuf, sizeof(petBuf)) : 0;
  // Settings: read whatever size was stored (older builds may differ) into a
  // zeroed struct, then trust it only if the magic matches.
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
  if (!haveSet && petLen == sizeof(State)) {
    // A pet raised with the P1 serial console: adopt it.
    std::memset(&set, 0, sizeof(set));
    set.magic = SETTINGS_MAGIC;
    set.version = 1;
    set.brightness = 1;
    set.timeout = 1;
    set.hasPet = 1;
    set.caseNo = 417;
    std::strcpy(set.name, "Specimen");
    haveSet = true;
  }
  app.begin(millis(), petLen ? petBuf : nullptr, petLen, haveSet ? &set : nullptr);
  Serial.printf("CRYPTLINGS up. touch %s, clock %s, wake %s, free heap %u\n", touch ? "ok" : "NOT FOUND",
                haveTime ? "NTP" : "UNVERIFIED",
                esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1 ? "button" : "power on",
                (unsigned)ESP.getFreeHeap());
}

void loop() {
  while (Serial.available()) console((char)Serial.read());
  uint32_t now = millis();
  TouchIn t{false, 0, 0};

  // ---- the button: wake, tap to sleep, hold to power off ------------------------
  // The external wake button (GPIO7) does everything; BOOT can wake and sleep the
  // screen but cannot power off (it cannot wake the chip from soft power off).
  static bool btnWas = false, btnWoke = false, btnCanOff = false, btnIgnore = true;
  static uint32_t btnDownAt = 0;
  bool wakeBtn = board::wakeButtonPressed();
  bool btn = wakeBtn || board::bootPressed();
  if (btnIgnore && !btn) btnIgnore = false;   // still held from the press that woke the chip
  if (btn && !btnWas && !btnIgnore) {
    btnDownAt = now;
    btnCanOff = wakeBtn;
    btnWoke = !app.screenOn();
    if (btnWoke) { t.down = true; t.x = 2; t.y = 2; }   // a swallowed touch wakes the app
  }
  if (btn && !btnIgnore && btnCanOff) {
    uint32_t held = now - btnDownAt;
    if (held > HOLD_SHOW_MS) app.setPowerHold((float)(held - HOLD_SHOW_MS) / (HOLD_OFF_MS - HOLD_SHOW_MS));
    if (held >= HOLD_OFF_MS) softPowerOff();
  }
  if (!btn && btnWas && !btnIgnore) {
    if (!btnWoke && now - btnDownAt < HOLD_SHOW_MS) app.sleepNow();   // short tap: lights out
    app.setPowerHold(0);
  }
  btnWas = btn;

  // ---- touch: fully off while the screen is dark (the button wakes it) ------------
  if (app.screenOn()) {
    if (!touchOn) { board::touchWake(); touchOn = true; }
    int x, y;
    if (!t.down && board::touchRead(x, y)) { t.down = true; t.x = x; t.y = y; }
  } else if (WAKE_BUTTON_PIN >= 0) {
    if (touchOn) { board::touchSleep(); touchOn = false; }
  } else {
    int x, y;                               // no wake button wired: touch still wakes it
    if (!t.down && board::touchRead(x, y)) { t.down = true; t.x = x; t.y = y; }
  }

  // ---- app + display ------------------------------------------------------------------
  if (app.update(now, t)) {
    if (!displayOn) { board::displayWake(); displayOn = true; }
    app.render();
    board::displayPush(FB);
  }
  if (!app.screenOn() && displayOn) { board::displaySleep(); displayOn = false; }
  board::buzzerService();

  // ---- battery: once a minute with the screen on, never while asleep -----------------
  // (the first read after waking happens right away, since the last one is old)
  if (app.screenOn() && (battCache == -2 || now - battAt > 60000UL)) {
    battCache = board::batteryPercent();
    battAt = now;
    static uint8_t empty = 0;
    if (battCache == 0) { if (++empty >= 2) softPowerOff(); }   // protect the LiPo, keep the save
    else empty = 0;
  }

  // ---- rest -----------------------------------------------------------------------------
  if (app.screenOn() || btn) {
    delay(10);
  } else if (app.settings().saver && !board::buzzerBusy() && !Serial) {
    board::lightSleep(60000);               // wakes on the button, BOOT, or once a minute
  } else {
    delay(120);
  }
}
