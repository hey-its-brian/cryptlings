// CRYPTLINGS P1 firmware: the core engine on the board, driven over serial.
// No display yet (that is P3). Use the serial monitor to raise a pet headless
// and feel out the tuning on real hardware and real time.
//
// Commands (115200 baud): see printHelp().
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>

#include "cryptlings_core.h"

using namespace cryptlings;

#ifndef TZ_RULE
#define TZ_RULE "EST5EDT,M3.2.0,M11.1.0"   // America/New_York
#endif

static Pet pet;
static Preferences prefs;
static uint32_t simOffset = 0;              // fast-forward for testing ('w' command)
static uint32_t lastSave = 0;
static bool haveTime = false;

static uint32_t nowUnix() {
  time_t t = time(nullptr);
  return (uint32_t)t + simOffset;
}

static int32_t utcOffset(uint32_t now) {
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
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
#endif
  haveTime = time(nullptr) >= 1700000000;
  if (!haveTime) {
    // No NTP: resume from the last saved time (the clock is UNVERIFIED).
    uint32_t last = prefs.getUInt("lastEpoch", 1792447200u);
    struct timeval tv = {(time_t)last, 0};
    settimeofday(&tv, nullptr);
  }
}

static void save() {
  uint8_t buf[sizeof(State)];
  size_t n = pet.serialize(buf, sizeof(buf));
  prefs.putBytes("pet", buf, n);
  prefs.putUInt("lastEpoch", nowUnix());
  lastSave = millis();
}

static void printStatus() {
  const State& s = pet.state();
  uint32_t now = nowUnix();
  Serial.printf("\n%s  %s  local %.2fh  %s%s%s\n", speciesName((Species)s.species),
                formName((Form)s.form), pet.localHour(now), pet.awake(now) ? "awake" : "asleep",
                pet.inQuiet(now) ? " (quiet hours)" : "", s.hibernating ? " HIBERNATING" : "");
  Serial.printf("belly %3.0f  cheer %3.0f  rest %3.0f  myst %3.0f  tidy %3.0f", s.stat[0], s.stat[1],
                s.stat[2], s.stat[3], s.stat[4]);
  if ((Species)s.species == Species::NESSIE) Serial.printf("  loch %3.0f", s.loch);
  Serial.printf("\nfork score %.1f -> %s   moon %.2f   treats left %u   clock %s\n",
                pet.forkScore(now), formName(pet.forkPreview(now)), Pet::moonPhase(now), s.treatsLeft,
                haveTime ? "NTP" : "UNVERIFIED");
}

static void printHelp() {
  Serial.println(F("\nCRYPTLINGS P1 serial console"));
  Serial.println(F(" new egg: ! then 1 mothman  2 bigfoot  3 nessie  4 nightcrawler   R release adult"));
  Serial.println(F(" care: f feed  t treat  r forage  p pat  c cuddle  n play  g play(great)"));
  Serial.println(F("       h hide  x clean  b scrub  l lights  m lamp on  M lamp off"));
  Serial.println(F("       k loch top-up  a walk  y dodge hiker  Y miss hiker"));
  Serial.println(F("       pants: q corduroy  e pinstripe  u pajama  i cargo"));
  Serial.println(F(" misc: s status  w +1 hour  W +1 day  d dig out  ? help"));
}

static const char* resultName(Result r) {
  switch (r) {
    case Result::OK: return "ok";
    case Result::WOKE_IT: return "ok (woke it up)";
    case Result::COOLDOWN: return "too soon";
    case Result::NOT_NOW: return "not now";
    case Result::HIBERNATING: return "hibernating, dig it out (d)";
  }
  return "?";
}

static bool armedNew = false;   // '!' then 1..4 starts a new egg

static void handle(char c) {
  if (c == '\r' || c == '\n' || c == ' ' || c == '\t') return;
  uint32_t now = nowUnix();
  pet.state().utcOffsetSec = utcOffset(now);
  Result r = Result::OK;
  bool acted = true;
  if (c == '!') { armedNew = true; Serial.println(F("New egg: press 1..4 (this replaces the current pet)")); return; }
  if (armedNew) {
    armedNew = false;
    if (c >= '1' && c <= '4') {
      bool first = pet.state().raisedMask == 0;
      pet.spawnEgg((Species)(c - '1'), now, utcOffset(now), first);
      Serial.printf("New egg: %s\n", speciesName((Species)(c - '1')));
      save();
    }
    return;
  }
  if (c == 'R') {
    Form f = pet.release(now);
    if (f == Form::EGG) { Serial.println(F("Only adults can be released.")); return; }
    Serial.printf("Released into the fog as %s. Case closed. Press ! then 1..4 for a new egg.\n", formName(f));
    save();
    return;
  }
  bool care = c != 's' && c != '?' && c != 'w' && c != 'W' && c != 'd';
  if (care) pet.beginSession(now);
  switch (c) {
    case 'f': r = pet.feed(now, Food::STAPLE); break;
    case 't': r = pet.feed(now, Food::TREAT); break;
    case 'r': r = pet.feed(now, Food::FORAGE); break;
    case 'p': r = pet.pat(now); break;
    case 'c': r = pet.cuddle(now); break;
    case 'n': r = pet.play(now, false); break;
    case 'g': r = pet.play(now, true); break;
    case 'h': r = pet.hide(now); break;
    case 'x': r = pet.clean(now); break;
    case 'b': r = pet.scrub(now); break;
    case 'l': r = pet.lights(now); break;
    case 'm': r = pet.setLamp(now, true); break;
    case 'M': r = pet.setLamp(now, false); break;
    case 'k': r = pet.topUpLoch(now); break;
    case 'a': r = pet.walk(now); break;
    case 'y': r = pet.resolveHiker(now, true); break;
    case 'Y': r = pet.resolveHiker(now, false); break;
    case 'd': pet.digOut(now); break;
    case 'w': simOffset += 3600; pet.tick(nowUnix()); break;
    case 'W': simOffset += 86400; pet.tick(nowUnix()); break;
    case 's': acted = false; printStatus(); break;
    case '?': acted = false; printHelp(); break;
    case 'q': r = pet.wearPants(now, 0); break;
    case 'e': r = pet.wearPants(now, 1); break;
    case 'u': r = pet.wearPants(now, 2); break;
    case 'i': r = pet.wearPants(now, 3); break;
    default: return;
  }
  if (acted) { Serial.printf("[%c] %s\n", c, resultName(r)); printStatus(); save(); }
}

static void drainEvents() {
  static const char* names[] = {"", "HATCHED", "EVOLVED", "EVIDENCE", "HIKER! (y hide / Y too late)",
                                "hiker got a photo", "dizzy", "missed walk", "went to sleep",
                                "woke at dusk", "restless: something out there is calling", "made a mess"};
  Event e;
  while (pet.nextEvent(e)) {
    Serial.printf("** %s", names[(int)e.kind]);
    if (e.kind == EventKind::EVOLVED || e.kind == EventKind::HATCHED) Serial.printf(" -> %s", formName((Form)e.a));
    if (e.kind == EventKind::EVIDENCE) Serial.printf(" item %u tier %u", e.a, e.b);
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  prefs.begin("cryptlings", false);
  syncTime();
  uint32_t now = nowUnix();
  uint8_t buf[sizeof(State)];
  size_t n = prefs.getBytes("pet", buf, sizeof(buf));
  if (n == sizeof(State) && pet.load(buf, n)) {
    pet.state().utcOffsetSec = utcOffset(now);
    pet.catchUp(now);
    Serial.println(F("Pet restored, catch-up applied."));
  } else {
    pet.spawnEgg(Species::MOTHMAN, now, utcOffset(now), true);
    Serial.println(F("No saved pet. Started a Mothman egg (press ! then 1..4 to choose)."));
  }
  printHelp();
  printStatus();
}

void loop() {
  static uint32_t lastTick = 0;
  while (Serial.available()) handle((char)Serial.read());
  if (millis() - lastTick > 5000) {
    lastTick = millis();
    uint32_t now = nowUnix();
    pet.state().utcOffsetSec = utcOffset(now);   // follow DST changes
    pet.tick(now);
    drainEvents();
  }
  if (millis() - lastSave > 10UL * 60UL * 1000UL) save();
}
