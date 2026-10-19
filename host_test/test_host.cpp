// Host-side test and balance harness for the CRYPTLINGS core engine.
// Build and run from the repo root:  make -f host.mk test   (verbose trace: make -f host.mk trace)
// Simulates whole weeks minute by minute with different keeper styles and
// checks the outcomes the design brief promises.
#include "cryptlings_core.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

using namespace cryptlings;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("  FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static const uint32_t T0 = 1792447200u;   // 2026-10-19 18:00 local (UTC-4)
static const int32_t  OFF = -4 * 3600;

enum class Keeper { GOOD, CASUAL, LAZY, NEGLECT, SPOILER };
static const char* keeperName(Keeper k) {
  switch (k) { case Keeper::GOOD: return "good"; case Keeper::CASUAL: return "casual"; case Keeper::LAZY: return "lazy";
               case Keeper::NEGLECT: return "neglect"; case Keeper::SPOILER: return "spoiler"; }
  return "?";
}

struct Run {
  Form outcome = Form::EGG;
  float adultDay = -1;          // days from spawn to adult
  float minStatSeen = 100;
  int evolutionsOutsideWindow = 0;
  int evidence = 0;
  float score = 0;
  std::vector<float> stageDays;
};

static uint32_t lcg = 12345;
static float urand() { lcg = lcg * 1103515245u + 12345u; return ((lcg >> 8) & 0xFFFF) / 65536.0f; }

// One minute of the keeper's routine at local time (hh:mm).
static void keeperMinute(Pet& p, Keeper k, uint32_t now, int day, int hh, int mm) {
  Species sp = (Species)p.state().species;
  auto at = [&](int h, int m) { return hh == h && mm == m; };
  // Bigfoot: answering a hiker alert if the keeper is around in the evening.
  if (p.hikerPending(now) && hh >= 18 && hh < 23) {
    if (k == Keeper::GOOD || k == Keeper::SPOILER) p.resolveHiker(now, urand() < 0.7f);
    else if (k == Keeper::CASUAL && urand() < 0.02f) p.resolveHiker(now, urand() < 0.5f);
  }
  if (k == Keeper::NEGLECT) {
    if (day % 2 == 0 && at(20, 0)) { p.beginSession(now); p.feed(now, Food::STAPLE); }
    return;
  }
  if (k == Keeper::LAZY) {          // one quick check-in a night, no hiding
    if (at(21, 0)) { p.beginSession(now); p.feed(now, Food::STAPLE); p.play(now, false); p.clean(now);
                     if (sp == Species::NESSIE) p.topUpLoch(now);
                     if (sp == Species::NIGHTCRAWLER) p.walk(now); }
    return;
  }
  if (k == Keeper::CASUAL) {
    if (at(19, 0)) { p.beginSession(now); p.feed(now, Food::STAPLE); p.clean(now); p.play(now, false); p.pat(now);
                     if (sp == Species::NESSIE) p.topUpLoch(now);
                     if (sp == Species::NIGHTCRAWLER) p.walk(now); }
    if (at(22, 0)) { p.beginSession(now); p.feed(now, Food::STAPLE); p.hide(now); p.cuddle(now); }
    if (day % 2 == 1 && at(7, 0)) { p.beginSession(now); p.lights(now); }
    return;
  }
  // GOOD and SPOILER share the evening routine; SPOILER feeds treats.
  bool spoil = k == Keeper::SPOILER;
  auto meal = [&]() { if (spoil) p.feed(now, Food::TREAT); p.feed(now, Food::STAPLE); };
  if (at(18, 15)) { p.beginSession(now); meal(); p.pat(now); p.pat(now); p.clean(now);
                    if (sp == Species::NESSIE) p.topUpLoch(now);
                    if (sp == Species::NIGHTCRAWLER) p.wearPants(now, (uint8_t)(urand() * 4)); }
  if (at(19, 30)) { p.beginSession(now); p.play(now, urand() < 0.3f); p.hide(now);
                    if (sp == Species::NIGHTCRAWLER) p.walk(now);
                    if (sp == Species::MOTHMAN) p.setLamp(now, true); }
  if (at(19, 32) && sp == Species::MOTHMAN) p.setLamp(now, false);
  if (at(20, 45)) { p.beginSession(now); meal(); p.pat(now); p.scrub(now); }
  if (at(21, 45)) { p.beginSession(now); p.play(now, urand() < 0.3f); p.hide(now); }
  if (at(22, 40)) { p.beginSession(now); meal(); p.cuddle(now); p.clean(now); }
  if (at(6, 45))  { p.beginSession(now); p.lights(now); }
}

static Run simulate(Species sp, Keeper k, int days, bool verbose,
                    int offFromDay = -1, int offDays = 0) {
  Pet p;
  p.spawnEgg(sp, T0, OFF, true);
  p.beginSession(T0);
  p.warmEgg(T0);
  Run run;
  Form last = Form::EGG;
  bool off = false;
  for (int minute = 1; minute <= days * 1440; ++minute) {
    uint32_t now = T0 + (uint32_t)minute * 60u;
    int day = minute / 1440;
    int localMin = (18 * 60 + minute) % 1440;
    int hh = localMin / 60, mm = localMin % 60;

    if (offFromDay >= 0 && day >= offFromDay && day < offFromDay + offDays) { off = true; continue; }
    if (off) {                        // power back on: cold boot
      off = false;
      Form before = (Form)p.state().form;
      p.catchUp(now);
      CHECK((Form)p.state().form == before, "evolved during catch-up");
      if (p.state().hibernating) { if (verbose) std::printf("    day %d: hibernating, dug out\n", day); p.digOut(now); }
      if (verbose) std::printf("    day %d: back on, catch-up done (care %.0f)\n", day, p.careNow());
    }
    p.tick(now);
    keeperMinute(p, k, now, day, hh, mm);
    Event e;
    while (p.nextEvent(e)) if (e.kind == EventKind::EVIDENCE) ++run.evidence;

    Form f = (Form)p.state().form;
    if (f != last) {
      float d = minute / 1440.0f;
      run.stageDays.push_back(d);
      bool inWindow = (hh >= 20 && hh < 23);
      if (f != Form::WHISPER && !inWindow) ++run.evolutionsOutsideWindow;
      if (verbose) std::printf("    day %5.2f %02d:%02d  -> %s\n", d, hh, mm, formName(f));
      if (f >= Form::LEGEND) { run.adultDay = d; run.score = p.forkScore(now); }
      last = f;
    }
    if (f != Form::EGG && !p.state().hibernating)
      for (int i = 0; i < (int)Stat::COUNT; ++i) if (p.state().stat[i] < run.minStatSeen) run.minStatSeen = p.state().stat[i];
    if (verbose && hh == 22 && mm == 50 && f != Form::EGG) {
      const State& s = p.state();
      std::printf("      d%-2d 22:50 belly %3.0f cheer %3.0f rest %3.0f myst %3.0f tidy %3.0f%s  score %4.1f\n",
                  day, s.stat[0], s.stat[1], s.stat[2], s.stat[3], s.stat[4],
                  sp == Species::NESSIE ? "" : "", p.forkScore(now));
    }
  }
  run.outcome = (Form)p.state().form;
  return run;
}

static void balanceMatrix() {
  std::printf("\n== Balance matrix (20 days, first pet) ==\n");
  std::printf("%-20s %-8s %-13s %8s %6s %8s %6s\n", "species", "keeper", "outcome", "adult@d", "score", "minStat", "evid");
  for (int s = 0; s < (int)Species::COUNT; ++s) {
    for (Keeper k : {Keeper::GOOD, Keeper::CASUAL, Keeper::LAZY, Keeper::NEGLECT, Keeper::SPOILER}) {
      lcg = 777 + s * 31;
      Run r = simulate((Species)s, k, 20, false);
      std::printf("%-20s %-8s %-13s %8.2f %6.1f %8.1f %6d\n", speciesName((Species)s), keeperName(k),
                  formName(r.outcome), r.adultDay, r.score, r.minStatSeen, r.evidence);
      CHECK(r.evolutionsOutsideWindow == 0, "%s/%s evolved outside the evening window",
            speciesName((Species)s), keeperName(k));
      if (k == Keeper::GOOD) {
        CHECK(r.outcome == Form::LEGEND, "good keeper should raise a LEGEND (%s)", speciesName((Species)s));
        float lo = s == (int)Species::NESSIE ? 8.0f : 6.5f, hi = s == (int)Species::NESSIE ? 10.5f : 9.0f;
        CHECK(r.adultDay >= lo && r.adultDay <= hi, "%s adult at day %.2f, want %.1f..%.1f",
              speciesName((Species)s), r.adultDay, lo, hi);
      }
      if (k == Keeper::NEGLECT) CHECK(r.outcome == Form::BLURRY,
                                      "neglect should grow up into BLURRY PHOTO (%s got %s)",
                                      speciesName((Species)s), formName(r.outcome));
      if (k == Keeper::SPOILER) CHECK(r.outcome == Form::HOAX, "spoiler should reach HOAX (%s got %s)",
                                      speciesName((Species)s), formName(r.outcome));
    }
  }
}

static void testMoon() {
  std::printf("\n== Moon ==\n");
  float p = Pet::moonPhase(1706205240u);  // full moon 2024-01-25 17:54 UTC
  std::printf("  phase at known full moon: %.3f\n", p);
  CHECK(std::fabs(p - 0.5f) < 0.02f, "moon phase at a known full moon should be ~0.5");
  CHECK(Pet::nearFullMoon(1706205240u, 18.0f), "nearFullMoon should be true at full moon");
  std::printf("  phase today (2026-10-19 23:42 local): %.3f\n", Pet::moonPhase(T0 + 5 * 3600 + 42 * 60));
}

static void testSchedule() {
  std::printf("\n== Schedule ==\n");
  Pet p; p.spawnEgg(Species::BIGFOOT, T0, OFF, true);
  auto at = [](int dayOff, int h, int m) { return T0 + (uint32_t)(dayOff * 86400 + (h - 18) * 3600 + m * 60); };
  CHECK(p.awake(at(0, 19, 0)), "awake at 19:00");
  CHECK(p.awake(at(1, 2, 0)), "awake at 02:00");
  CHECK(p.awake(at(1, 7, 30)), "untucked: still up at 07:30");
  CHECK(!p.awake(at(1, 9, 0)), "asleep at 09:00");
  CHECK(p.inQuiet(at(1, 1, 0)) && !p.inQuiet(at(0, 22, 0)), "quiet hours 23:00 to 07:00");
}

static void testWakePenaltyAndNoDeath() {
  std::printf("\n== Day wake penalty, catch-up floor, hibernation ==\n");
  Pet p; p.spawnEgg(Species::MOTHMAN, T0, OFF, true);
  uint32_t now = T0 + 11 * 60;  p.tick(now);              // hatches (first pet)
  CHECK((Form)p.state().form == Form::WHISPER, "first pet hatches after 10 min");
  uint32_t noon = T0 + 18 * 3600;  p.tick(noon);
  float c0 = p.state().stat[(int)Stat::CHEER];
  Result r = p.feed(noon, Food::STAPLE);
  CHECK(r == Result::WOKE_IT, "feeding at noon wakes it");
  CHECK(p.state().stat[(int)Stat::CHEER] < c0, "waking costs cheer");
  CHECK(p.awake(noon + 600) && !p.awake(noon + 25 * 60), "awake for 20 min then back to sleep");

  // Five days off: capped catch-up, floor, hibernation.
  uint32_t back = noon + 5 * 86400;
  p.catchUp(back);
  CHECK(p.state().hibernating == 1, "over 72h off -> hibernating");
  for (int i = 0; i < (int)Stat::COUNT; ++i) CHECK(p.state().stat[i] >= 10.0f - 0.01f || i == (int)Stat::MYST,
                                                  "catch-up floor holds for stat %d (%.1f)", i, p.state().stat[i]);
  CHECK(p.feed(back, Food::STAPLE) == Result::HIBERNATING, "must dig out first");
  p.digOut(back);
  CHECK(std::fabs(p.state().stat[0] - 50.0f) < 0.01f, "dig out resets stats to 50");
  CHECK((Form)p.state().form == Form::WHISPER, "no evolution while away");
}

static void testPersistence() {
  std::printf("\n== Persistence ==\n");
  Pet a; a.spawnEgg(Species::NESSIE, T0, OFF, true);
  a.tick(T0 + 3600);
  a.feed(T0 + 3600, Food::STAPLE);
  uint8_t buf[sizeof(State)];
  size_t n = a.serialize(buf, sizeof(buf));
  CHECK(n == sizeof(State), "serialize size");
  Pet b; CHECK(b.load(buf, n), "load roundtrip");
  CHECK(std::fabs(b.state().stat[0] - a.state().stat[0]) < 1e-6f, "belly survives roundtrip");
  buf[20] ^= 0x5A;
  Pet c; CHECK(!c.load(buf, n), "corrupted buffer rejected by crc");
  std::printf("  State is %zu bytes\n", sizeof(State));
}

static void testClockBack() {
  std::printf("\n== Clock jumps ==\n");
  Pet p; p.spawnEgg(Species::BIGFOOT, T0, OFF, true);
  p.tick(T0 + 3 * 3600);
  float b = p.state().stat[0];
  p.tick(T0 + 3600);                               // clock went backwards
  CHECK(std::fabs(p.state().stat[0] - b) < 1e-6f, "backward jump changes nothing");
}

static void testQuirks() {
  std::printf("\n== Quirks ==\n");
  // Mothman dizzy
  Pet m; m.spawnEgg(Species::MOTHMAN, T0, OFF, true);
  uint32_t t = T0 + 20 * 60; m.tick(t);
  m.setLamp(t, true);
  for (int i = 1; i <= 5; ++i) m.tick(t + i * 60);
  CHECK(m.play(t + 5 * 60, false) == Result::NOT_NOW, "dizzy Mothman can't play");
  CHECK(m.play(t + 25 * 60, false) != Result::NOT_NOW, "dizziness wears off");
  // Nightcrawler pants and missed walk
  Pet n; n.spawnEgg(Species::NIGHTCRAWLER, T0, OFF, true);
  t = T0 + 20 * 60; n.tick(t);
  uint8_t want = n.state().pantsWanted;
  float c0 = n.state().stat[(int)Stat::CHEER];
  n.wearPants(t, want);
  CHECK(n.state().stat[(int)Stat::CHEER] > c0, "right pants cheer him up");
  CHECK(n.wearPants(t, want) == Result::COOLDOWN, "one outfit per night");
  n.tick(T0 + 20 * 3600);                                   // next noon: night 1 had walked=1 (first night)
  n.tick(T0 + 44 * 3600);                                   // noon after a night with no walk
  Event e; bool missed = false; while (n.nextEvent(e)) if (e.kind == EventKind::MISSED_WALK) missed = true;
  CHECK(missed, "missed walk is noticed");
  // Species gates
  CHECK(n.topUpLoch(T0 + 45 * 3600) == Result::NOT_NOW, "only Nessie has a loch");
  CHECK(n.feed(T0 + 45 * 3600, Food::FORAGE) == Result::NOT_NOW, "only Bigfoot forages");
}


static void testReviewRegressions() {
  std::printf("\n== Regressions from code review ==\n");
  // 1. A session at noon right before a care action must not double-roll the night.
  {
    Pet p; p.spawnEgg(Species::NIGHTCRAWLER, T0, OFF, true);
    uint32_t t = T0 + 20 * 60; p.tick(t);
    p.tick(T0 + 18 * 3600 - 60);                 // 11:59 next day, walked flag from first night = 1
    uint32_t noon = T0 + 18 * 3600 + 2;
    float c0 = p.state().stat[(int)Stat::CHEER];
    p.beginSession(noon); p.pat(noon);
    Event e; int missed = 0; while (p.nextEvent(e)) if (e.kind == EventKind::MISSED_WALK) ++missed;
    CHECK(missed == 0, "no missed-walk penalty after a walked night (got %d)", missed);
    CHECK(p.state().stat[(int)Stat::CHEER] >= c0 - 0.5f, "cheer not docked at noon rollover");
  }
  // 2. Clock set back must not trigger an instant evolution.
  {
    Pet p; p.spawnEgg(Species::BIGFOOT, T0, OFF, true);
    p.tick(T0 + 20 * 60);
    uint32_t t = T0 + 20 * 60;
    for (int i = 0; i < 6; ++i) { t += 3600; p.tick(t); }
    Form f0 = (Form)p.state().form;
    p.tick(t - 1800);                            // back 30 min
    p.tick(T0 + 26 * 3600 + 1800);               // a later in-window-ish tick, far short of 36h
    CHECK((Form)p.state().form == f0, "clock-back does not evolve early");
  }
  // 3. Hibernation time does not count as age.
  {
    Pet p; p.spawnEgg(Species::MOTHMAN, T0, OFF, true);
    p.tick(T0 + 20 * 60);
    uint32_t back = T0 + 5 * 86400;
    p.catchUp(back);
    CHECK(p.state().hibernating, "hibernating after 5 days");
    uint32_t later = back + 10 * 86400;
    p.tick(later);
    p.digOut(later);
    float age = (later - p.state().stageStart) / 3600.0f;
    CHECK(age <= 72.5f, "age advanced %.1fh, want <= 72h", age);
  }
  // 4. Catch-up floor holds against nightly penalties.
  {
    Pet p; p.spawnEgg(Species::NIGHTCRAWLER, T0, OFF, true);
    p.tick(T0 + 20 * 60);
    p.state().stat[(int)Stat::CHEER] = 12;
    p.catchUp(T0 + 20 * 60 + 48 * 3600);
    CHECK(p.state().stat[(int)Stat::CHEER] >= 10.0f - 0.01f, "cheer floored at 10 (got %.1f)",
          p.state().stat[(int)Stat::CHEER]);
  }
  // 7. Tucking in at 13:00 gives no Rest bonus and does not pre-tuck tomorrow.
  {
    Pet p; p.spawnEgg(Species::BIGFOOT, T0, OFF, true);
    p.tick(T0 + 20 * 60);
    uint32_t onePm = T0 + 19 * 3600;
    p.tick(onePm);
    float r0 = p.state().stat[(int)Stat::REST];
    p.lights(onePm);
    CHECK(p.state().stat[(int)Stat::REST] <= r0 + 0.01f, "no tuck bonus at 13:00");
    CHECK(!p.state().tucked, "not marked tucked at 13:00");
  }
  // 8. Evidence cannot be farmed.
  {
    Pet p; p.spawnEgg(Species::NESSIE, T0, OFF, true);
    uint32_t t = T0 + 20 * 60; p.tick(t);
    int ev = 0; Event e;
    for (int i = 0; i < 200; ++i) { p.topUpLoch(t + i); p.play(t + i, true); }
    while (p.nextEvent(e)) if (e.kind == EventKind::EVIDENCE) ++ev;
    CHECK(ev <= 4, "action evidence capped per night (got %d)", ev);
  }
}

int main(int argc, char**) {
  bool verbose = argc > 1;
  testMoon();
  testSchedule();
  testWakePenaltyAndNoDeath();
  testPersistence();
  testClockBack();
  testQuirks();
  testReviewRegressions();
  balanceMatrix();

  std::printf("\n== Good keeper, power off days 3-4 (Bigfoot) ==\n");
  lcg = 99;
  Run r = simulate(Species::BIGFOOT, Keeper::GOOD, 12, verbose, 3, 2);
  std::printf("  outcome %s at day %.2f, score %.1f\n", formName(r.outcome), r.adultDay, r.score);

  if (verbose) {
    std::printf("\n== Verbose: good Mothman ==\n"); lcg = 1; simulate(Species::MOTHMAN, Keeper::GOOD, 10, true);
    std::printf("\n== Verbose: casual Nessie ==\n"); lcg = 2; simulate(Species::NESSIE, Keeper::CASUAL, 12, true);
  }

  std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
