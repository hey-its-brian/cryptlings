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
    static int cached = -2;
    static uint32_t at = 0;
    if (cached == -2 || millis() - at > 30000) { cached = board::batteryPercent(); at = millis(); }
    return cached;
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
void setup() {
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
  Serial.printf("CRYPTLINGS up. touch %s, clock %s, free heap %u\n", touch ? "ok" : "NOT FOUND",
                haveTime ? "NTP" : "UNVERIFIED", (unsigned)ESP.getFreeHeap());
}

void loop() {
  while (Serial.available()) console((char)Serial.read());

  TouchIn t{false, 0, 0};
  int x, y;
  if (board::touchRead(x, y)) { t.down = true; t.x = x; t.y = y; }

  // BOOT button: a press while the screen is off wakes it (same as a touch).
  static bool bootWas = false;
  bool boot = board::bootPressed();
  if (boot && !bootWas && !app.screenOn()) { t.down = true; t.x = 2; t.y = 2; }
  bootWas = boot;

  if (app.update(millis(), t)) {
    app.render();
    board::displayPush(FB);
  }
  board::buzzerService();
  delay(app.screenOn() ? 10 : 120);   // poll slower with the screen off to save power
}
