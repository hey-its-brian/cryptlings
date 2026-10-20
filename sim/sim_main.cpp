// Desktop simulator: runs the real app code against a fake platform with a
// controllable clock and scripted touches, and writes screenshots (PPM).
// Build and run: make -f host.mk sim        rectangle build, build/shots/
//                make -f host.mk sim_round  round StopWatch build, build/shots_round/
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "app.h"
#include "gfx.h"

static gfx::Color FB[gfx::W * gfx::H];

struct SimPlatform : Platform {
  uint32_t clock = 1792461600u;   // 2026-10-19 22:00 local (UTC-4) = 02:00 UTC next day
  int32_t off = -4 * 3600;
  std::vector<uint8_t> pet;
  Settings set{};
  bool hasSet = false;
  int notes = 0;
  uint8_t bl = 255;
  int vibes = 0;
  bool hasAcc = false;
  float ax = 0, ay = 0, az = 1;
  uint32_t unixNow() override { return clock; }
  int32_t utcOffset(uint32_t) override { return off; }
  bool clockVerified() override { return true; }
  void setClock(uint32_t u) override { clock = u; }
  void play(const Note*, int n) override { notes += n; }
  void backlight(uint8_t l) override { bl = l; }
  int batteryPercent() override { return 86; }
  void savePet(const uint8_t* d, size_t n) override { pet.assign(d, d + n); }
  void saveSettings(const Settings& s) override { set = s; hasSet = true; }
  void vibrate(uint16_t) override { ++vibes; }
  bool accel(float& x, float& y, float& z) override {
    if (!hasAcc) return false;
    x = ax; y = ay; z = az;
    return true;
  }
};

static SimPlatform P;
static gfx::Canvas CV(FB);
static App* app;
static uint32_t ms = 1000;
static int shotNo = 0;
static std::string outDir = "build/shots";

static void frame(TouchIn t) {
  if (app->update(ms, t)) app->render();
}
static void run(uint32_t dur, TouchIn t = {false, 0, 0}) {
  uint32_t end = ms + dur;
  while (ms < end) { ms += 20; frame(t); }
}
static void advanceClock(uint32_t secs) {   // real time passes, app keeps ticking
  for (uint32_t s = 0; s < secs; s += 30) { P.clock += 30; ms += 30000; frame({false, 0, 0}); }
  if (!app->screenOn()) { run(60, {true, 2, 300}); run(60); }   // a touch wakes the screen
}
static void tap(int x, int y) { run(60, {true, x, y}); run(60); }
static void hold(int x, int y, uint32_t d) { run(d, {true, x, y}); run(60); }
static void swipe(int x0, int x1, int y) {
  for (int i = 0; i <= 8; ++i) { ms += 20; frame({true, x0 + (x1 - x0) * i / 8, y}); }
  run(60);
}
static void drag(int x, int y0, int y1) {
  for (int i = 0; i <= 10; ++i) { ms += 20; frame({true, x, y0 + (y1 - y0) * i / 10}); }
  run(60);
}
static void shot(const char* name, bool rerender = true) {
  if (rerender) app->render();
  char path[256];
  std::snprintf(path, sizeof(path), "%s/%02d_%s.ppm", outDir.c_str(), shotNo++, name);
  FILE* f = std::fopen(path, "wb");
  if (!f) { std::perror(path); std::exit(1); }
  std::fprintf(f, "P6 %d %d 255\n", gfx::W, gfx::H);
  for (int i = 0; i < gfx::W * gfx::H; ++i) {
    uint16_t c = FB[i];
    uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31)};
#if CRYPT_ROUND
    // The panel is round: show what is outside the glass as the case.
    int x = i % gfx::W, y = i / gfx::W;
    float dx = x - 232.5f, dy = y - 232.5f;
    if (dx * dx + dy * dy > 233.0f * 233.0f) { rgb[0] = 58; rgb[1] = 58; rgb[2] = 62; }
#endif
    std::fwrite(rgb, 1, 3, f);
  }
  std::fclose(f);
}

static void newApp(bool restore) {
  delete app;
  app = new App(P, CV);
  app->begin(ms, restore && !P.pet.empty() ? P.pet.data() : nullptr, P.pet.size(), P.hasSet ? &P.set : nullptr);
  run(100);
}

#if !CRYPT_ROUND
// Raise a fresh pet of one species straight to the DEN.
static void freshPet(int species) {
  P.pet.clear(); P.hasSet = false;
  newApp(false);
  tap(86, 160);                                    // skip boot
  for (int i = 0; i < species; ++i) tap(150, 110); // next egg
  hold(86, 290, 2200);                             // incubate
  tap(128, 290);                                   // USE IT
  run(200);
}

// Advance until the local clock reads hh:mm.
static void toLocal(int hh, int mm) {
  int64_t l = (int64_t)P.clock + P.off;
  int sec = (int)((l % 86400 + 86400) % 86400);
  int target = hh * 3600 + mm * 60;
  int d = target - sec;
  if (d <= 0) d += 86400;
  advanceClock((uint32_t)d);
}

int main(int argc, char** argv) {
  if (argc > 1) outDir = argv[1];
  std::string mk = "mkdir -p " + outDir;
  if (std::system(mk.c_str()) != 0) return 1;

  // ---- first boot and setup ----
  newApp(false);
  run(1500); shot("boot_typing");
  run(5000); shot("boot_done");
  tap(86, 160); run(300); shot("tray_mothman");
  swipe(140, 30, 120); run(300); shot("tray_bigfoot");
  tap(150, 110); run(300); shot("tray_nessie");
  tap(150, 110); run(300); shot("tray_nightcrawler");
  tap(150, 110); run(300);                          // back to Mothman
  run(1000, {true, 86, 290}); shot("tray_holding");
  run(1200, {true, 86, 290}); run(100);
  shot("name");
  tap(40, 290); shot("name_another");
  tap(128, 290); run(300); shot("den_egg");
  tap(86, 110); shot("den_egg_warm");

  // ---- hatch (first pet: 10 minutes) ----
  advanceClock(11 * 60);
  run(500); shot("hatch_developing");
  run(3000); shot("hatch_reveal");
  tap(86, 160); run(3000); shot("den_whisper_night");
  tap(86, 110); shot("den_pat");
  hold(86, 110, 900); shot("den_cuddle");
  tap(128, 290); run(200); shot("care_tab");
  tap(86, 70 + 54 * 0); run(200); shot("after_feed");
  tap(128, 290); run(200);
  tap(86, 66 + 54 * 2 + 20); run(500); shot("game_waltz_start");
  for (int i = 0; i < 40; ++i) { ms += 100; frame({true, 86 + (i % 7) * 4, 180 + (i % 5) * 6}); }
  shot("game_waltz_mid");
  run(19000, {true, 120, 200}); run(200); shot("game_result");
  tap(86, 160); run(200);
  tap(105, 30); run(200); shot("file_tab");
  drag(86, 280, 60); drag(86, 280, 60); shot("file_scrolled");
  tap(150, 30); run(200); shot("more_tab");
  drag(86, 280, 100); shot("more_scrolled");
  tap(20, 30); run(200);

  // ---- daytime: asleep ----
  advanceClock(10 * 3600);   // 08:00
  tap(86, 160); run(400); shot("den_day_asleep");

  // ---- the other species ----
  for (int sp = 1; sp < 4; ++sp) {
    freshPet(sp);
    shot(sp == 1 ? "bigfoot_egg" : sp == 2 ? "nessie_egg" : "nightcrawler_egg");
    advanceClock(11 * 60);
    run(3600); tap(86, 160); run(500);
    shot(sp == 1 ? "bigfoot_whisper" : sp == 2 ? "nessie_whisper" : "nightcrawler_whisper");
    tap(128, 290); run(200);
    shot(sp == 1 ? "bigfoot_care" : sp == 2 ? "nessie_care" : "nightcrawler_care");
    tap(86, 66 + 54 * 2 + 20); run(2000);
    if (sp == 1) for (int i = 0; i < 30; ++i) { tap(46 + (i % 2) * 80, 96 + (i % 3) * 72); }
    if (sp == 2) for (int i = 0; i < 30; ++i) { tap(60 + (i % 5) * 20, 210 + (i % 4) * 20); }
    if (sp == 3) for (int i = 0; i < 30; ++i) { tap(i % 2 ? 130 : 40, 200); run(300); }
    shot(sp == 1 ? "game_forage" : sp == 2 ? "game_hop" : "game_stroll");
    run(21000); shot(sp == 1 ? "bigfoot_game_result" : sp == 2 ? "nessie_game_result" : "nightcrawler_game_result");
    tap(86, 160); run(200);
  }
  // Nightcrawler pants overlay
  tap(128, 290); run(200); drag(86, 280, 100); run(100);
  shot("nightcrawler_care_scrolled");

  // ---- grown-up states via the engine ----
  cryptlings::State& st = app->pet().state();
  st.form = (uint8_t)cryptlings::Form::SIGHTING;
  st.stagePhotos = 0x1F;
  tap(20, 30); run(300); shot("den_sighting");
  st.form = (uint8_t)cryptlings::Form::LEGEND;
  st.stagePhotos |= 1 << 4;
  st.evidence[3][0] = 3; st.evidence[3][7] = 1; st.evidence[3][13] = 5;
  tap(20, 30); run(300); shot("den_legend");
  tap(105, 30); run(200); drag(86, 300, 40); drag(86, 300, 40); drag(86, 300, 40); drag(86, 300, 40); shot("file_legend_release");

  // Blurry Photo of Bigfoot and a hiker
  freshPet(1);
  advanceClock(11 * 60); run(3600); tap(86, 160);
  app->pet().state().form = (uint8_t)cryptlings::Form::BLURRY;
  run(300); shot("den_blurry_bigfoot");

  // Hibernation after a long time off
  P.clock += 6 * 86400;
  newApp(true);
  tap(86, 160); run(500); shot("burrow");
  hold(86, 252, 1700); run(300); shot("dug_out");


  // ---- more flows ----
  // Legend reveal with the fork card
  freshPet(0);
  advanceClock(11 * 60); run(3600); tap(86, 160); run(300);
  {
    cryptlings::State& s2 = app->pet().state();
    toLocal(20, 30);
    s2.form = (uint8_t)cryptlings::Form::SIGHTING;
    s2.stageStart = P.clock - 200 * 3600;
    s2.weight = 100; s2.careW = 88 * 100; s2.mystW = 81 * 100; s2.neglectHours = 2;
    for (int i = 0; i < 5; ++i) s2.stat[i] = 85;
    advanceClock(60);
    run(1000); shot("legend_developing");
    run(3000); shot("legend_reveal_card");
    tap(86, 160); run(300);
  }
  // Clock + reset overlays from MORE
  tap(150, 30); run(200); drag(86, 300, 100); run(100); shot("more_rows");
  tap(86, 228); run(200); shot("clock_overlay");
  tap(50, 140); tap(86, 294); run(200);
  drag(86, 300, 100); tap(86, 285); run(200); shot("reset_confirm");
  tap(86, 280); run(200);
  // Bigfoot hiker alert
  freshPet(1);
  advanceClock(11 * 60); run(3600); tap(86, 160); run(300);
  toLocal(21, 0); run(300);
  app->pet().state().hikerAt[0] = P.clock + 40;
  app->pet().state().hikerAt[1] = 0;
  advanceClock(60); run(1500); shot("hiker_alert");
  tap(86, 222); run(300); shot("hiker_dodged");
  // Nightcrawler pants
  freshPet(3);
  advanceClock(11 * 60); run(3600); tap(86, 160); run(300);
  toLocal(21, 0); run(300);
  tap(128, 290); run(200); drag(86, 300, 120); run(100);
  shot("nc_care_bottom");
  tap(86, 270); run(200); shot("pants_overlay");
  tap(86, 110); run(300); shot("pants_chosen");
  // Release an adult
  app->pet().state().form = (uint8_t)cryptlings::Form::LEGEND;
  app->pet().state().restless = 1;
  tap(105, 30); run(200);
  for (int i = 0; i < 6; ++i) drag(86, 300, 40);
  shot("file_release");
  hold(86, 285, 1700); run(300); shot("farewell");
  tap(86, 160); run(500); shot("tray_after_release");

  // ---- regression checks from the code review ----
  int fails = 0;
  {
    // Release then reboot keeps the collection.
    freshPet(1);
    advanceClock(11 * 60); run(3600); tap(86, 160); run(300);
    app->pet().state().evidence[1][0] = 4;
    app->pet().state().form = (uint8_t)cryptlings::Form::LEGEND;
    app->forceSave();
    tap(105, 30); run(200);
    for (int i = 0; i < 6; ++i) drag(86, 300, 40);
    hold(86, 285, 1700); run(300); tap(86, 160); run(300);
    newApp(true);
    if (app->pet().state().evidence[1][0] != 4) { std::printf("FAIL: evidence lost after release+reboot\n"); ++fails; }
    // Clock moved forward by hand: capped catch-up, floor, hibernation.
    freshPet(0);
    advanceClock(11 * 60); run(3600); tap(86, 160); run(300);
    for (int i = 0; i < 5; ++i) app->pet().state().stat[i] = 60;
    tap(150, 30); run(200); drag(86, 300, 100); run(100);
    tap(86, 228); run(200);
    for (int i = 0; i < 4; ++i) { tap(48, 240); }    // +1 day x4
    tap(86, 294); run(300);
    const cryptlings::State& cs = app->pet().state();
    if (!cs.hibernating) { std::printf("FAIL: 4 days forward did not hibernate\n"); ++fails; }
    for (int i = 0; i < 5; ++i) if (cs.stat[i] < 9.9f) { std::printf("FAIL: stat %d below floor (%.1f)\n", i, cs.stat[i]); ++fails; }
    shot("clock_forward_burrow");
  }
  // Battery saver row + power-off cards
  freshPet(2);
  tap(150, 30); run(200); drag(86, 300, 160); run(100); shot("more_with_saver");
  app->setPowerHold(0.55f); run(100); shot("power_hold");
  app->setPowerHold(0); app->renderPowerOff(); shot("powered_off", false);
  std::printf("regression checks: %s\n", fails ? "FAILED" : "passed");
  std::printf("wrote %d screenshots to %s (notes played: %d)\n", shotNo, outDir.c_str(), P.notes);
  return 0;
}
#endif  // !CRYPT_ROUND

#if CRYPT_ROUND
// ---- round StopWatch script ------------------------------------------------------
// Controls are found by asking the app what is under each point (hitAt), so
// this script does not hard-code the round layout.
static bool where(uint8_t id, int idx, int& cx, int& cy) {
  int x0 = 9999, y0 = 9999, x1 = -1, y1 = -1;
  for (int y = 0; y < gfx::H; y += 3)
    for (int x = 0; x < gfx::W; x += 3) {
      Hit h = app->hitAt(x, y);
      if (h.id != id || (idx >= 0 && h.idx != idx)) continue;
      if (x < x0) x0 = x;
      if (x > x1) x1 = x;
      if (y < y0) y0 = y;
      if (y > y1) y1 = y;
    }
  if (x1 < 0) return false;
  cx = (x0 + x1) / 2; cy = (y0 + y1) / 2;
  return true;
}
static int missing = 0;
static void tapOn(uint8_t id, int idx = -1) {
  int x, y;
  if (!where(id, idx, x, y)) { std::printf("MISSING hit %d/%d on shot %d\n", id, idx, shotNo); ++missing; return; }
  tap(x, y);
}
static void holdOn(uint8_t id, uint32_t d) {
  int x, y;
  if (!where(id, -1, x, y)) { std::printf("MISSING hold %d on shot %d\n", id, shotNo); ++missing; return; }
  hold(x, y, d);
}
static void tapCenter() { tap(233, 233); }
static void scrollDown(int times = 1) { for (int i = 0; i < times; ++i) drag(233, 380, 120); }

static void freshPet(int species) {
  P.pet.clear(); P.hasSet = false;
  newApp(false);
  tapCenter();                                  // skip boot
  for (int i = 0; i < species; ++i) tapOn(HIT_NEXT);
  holdOn(HIT_HOLD_INCUBATE, 2200);
  tapOn(HIT_USE);
  run(200);
}
static void toLocal(int hh, int mm) {
  int64_t l = (int64_t)P.clock + P.off;
  int sec = (int)((l % 86400 + 86400) % 86400);
  int d = hh * 3600 + mm * 60 - sec;
  if (d <= 0) d += 86400;
  advanceClock((uint32_t)d);
}
static void hatchNow() { advanceClock(11 * 60); run(3600); tapCenter(); run(300); }

int main(int argc, char** argv) {
  outDir = "build/shots_round";
  if (argc > 1) outDir = argv[1];
  std::string mk = "mkdir -p " + outDir;
  if (std::system(mk.c_str()) != 0) return 1;
  int fails = 0;

  // ---- first boot and setup ----
  newApp(false);
  run(1500); shot("boot_typing");
  run(5000); shot("boot_done");
  tapCenter(); run(300); shot("tray_mothman");
  swipe(330, 130, 300); run(300); shot("tray_bigfoot");
  tapOn(HIT_NEXT); run(300); shot("tray_nessie");
  tapOn(HIT_NEXT); run(300); shot("tray_nightcrawler");
  tapOn(HIT_NEXT); run(300);                          // back to Mothman
  {
    int x = 0, y = 0;
    if (!where(HIT_HOLD_INCUBATE, -1, x, y)) { std::printf("MISSING egg hold\n"); ++missing; }
    run(1000, {true, x, y}); shot("tray_holding");
    run(1200, {true, x, y}); run(100);
  }
  shot("name");
  tapOn(HIT_ANOTHER); shot("name_another");
  tapOn(HIT_USE); run(300); shot("den_egg");
  tapOn(HIT_PET); shot("den_egg_warm");

  // ---- hatch (first pet: 10 minutes) ----
  advanceClock(11 * 60);
  run(500); shot("hatch_developing");
  run(3000); shot("hatch_reveal");
  tapCenter(); run(3000); shot("den_whisper_night");
  tapOn(HIT_PET); shot("den_pat");
  holdOn(HIT_PET, 900); shot("den_cuddle");
  tapOn(HIT_TAB, 1); run(200); shot("care_tab");
  tapOn(HIT_ROW, 0); run(200); shot("after_feed");
  tapOn(HIT_TAB, 1); run(200);
  tapOn(HIT_ROW, 2); run(500); shot("game_waltz_start");
  for (int i = 0; i < 40; ++i) { ms += 100; frame({true, 233 + (i % 7) * 8, 260 + (i % 5) * 10}); }
  shot("game_waltz_mid");
  run(19000, {true, 300, 280}); run(200); shot("game_result");
  tapCenter(); run(200);
  tapOn(HIT_TAB, 2); run(200); shot("file_tab");
  scrollDown(2); shot("file_scrolled");
  tapOn(HIT_TAB, 3); run(200); shot("more_tab");
  scrollDown(1); shot("more_scrolled");
  tapOn(HIT_TAB, 0); run(200);

  // ---- daytime: asleep ----
  advanceClock(10 * 3600);   // 08:00
  tapCenter(); run(400); shot("den_day_asleep");

  // ---- the other species ----
  for (int sp = 1; sp < 4; ++sp) {
    freshPet(sp);
    shot(sp == 1 ? "bigfoot_egg" : sp == 2 ? "nessie_egg" : "nightcrawler_egg");
    hatchNow();
    shot(sp == 1 ? "bigfoot_whisper" : sp == 2 ? "nessie_whisper" : "nightcrawler_whisper");
    tapOn(HIT_TAB, 1); run(200);
    shot(sp == 1 ? "bigfoot_care" : sp == 2 ? "nessie_care" : "nightcrawler_care");
    tapOn(HIT_ROW, 2); run(2000);
    if (sp == 1) for (int i = 0; i < 30; ++i) tap(i % 2 ? 306 : 160, 150 + (i % 3) * 92);
    if (sp == 2) for (int i = 0; i < 30; ++i) tap(140 + (i % 5) * 40, 300 + (i % 4) * 26);
    if (sp == 3) for (int i = 0; i < 30; ++i) { tap(i % 2 ? 340 : 120, 250); run(300); }
    shot(sp == 1 ? "game_forage" : sp == 2 ? "game_hop" : "game_stroll");
    run(21000); shot(sp == 1 ? "bigfoot_game_result" : sp == 2 ? "nessie_game_result" : "nightcrawler_game_result");
    tapCenter(); run(200);
  }
  tapOn(HIT_TAB, 1); run(200); scrollDown(1); run(100);
  shot("nightcrawler_care_scrolled");

  // ---- grown-up states via the engine ----
  cryptlings::State& st = app->pet().state();
  st.form = (uint8_t)cryptlings::Form::SIGHTING;
  st.stagePhotos = 0x1F;
  tapOn(HIT_TAB, 0); run(300); shot("den_sighting");
  st.form = (uint8_t)cryptlings::Form::LEGEND;
  st.stagePhotos |= 1 << 4;
  st.evidence[3][0] = 3; st.evidence[3][7] = 1; st.evidence[3][13] = 5;
  run(300); shot("den_legend");
  tapOn(HIT_TAB, 2); run(200); scrollDown(6); shot("file_legend_release");

  // Blurry Bigfoot
  freshPet(1);
  hatchNow();
  app->pet().state().form = (uint8_t)cryptlings::Form::BLURRY;
  run(300); shot("den_blurry_bigfoot");

  // Nessie's loch on the rim
  freshPet(2);
  hatchNow();
  app->pet().state().loch = 35;
  run(300); shot("den_nessie_loch");

  // Hibernation after a long time off
  P.clock += 6 * 86400;
  newApp(true);
  tapCenter(); run(1500); shot("burrow");
  {
    int x = 0, y = 0;
    if (!where(HIT_HOLD_DIG, -1, x, y)) { std::printf("MISSING burrow hold\n"); ++missing; }
    run(800, {true, x, y}); shot("burrow_holding");
    run(900, {true, x, y}); run(300);
  }
  shot("dug_out");
  if (app->pet().state().hibernating) { std::printf("FAIL: holding the burrow did not dig it out\n"); ++fails; }

  // Legend reveal with the fork card
  freshPet(0);
  hatchNow();
  {
    cryptlings::State& s2 = app->pet().state();
    toLocal(20, 30);
    s2.form = (uint8_t)cryptlings::Form::SIGHTING;
    s2.stageStart = P.clock - 200 * 3600;
    s2.weight = 100; s2.careW = 88 * 100; s2.mystW = 81 * 100; s2.neglectHours = 2;
    for (int i = 0; i < 5; ++i) s2.stat[i] = 85;
    advanceClock(60);
    run(1000); shot("legend_developing");
    run(3000); shot("legend_reveal_card");
    tapCenter(); run(300);
  }
  // Clock + reset overlays from MORE
  tapOn(HIT_TAB, 3); run(200); scrollDown(2); run(100); shot("more_rows");
  tapOn(HIT_ROW, 4); run(200); shot("clock_overlay");
  tapOn(HIT_CLOCK, 0); tapOn(HIT_DONE); run(200);
  scrollDown(2); tapOn(HIT_ROW, 5); run(200); shot("reset_confirm");
  tapOn(HIT_CANCEL); run(200);
  // Bigfoot hiker alert
  freshPet(1);
  hatchNow();
  toLocal(21, 0); run(300);
  app->pet().state().hikerAt[0] = P.clock + 40;
  app->pet().state().hikerAt[1] = 0;
  int vib0 = P.vibes;
  advanceClock(60); run(1500); shot("hiker_alert");
  if (P.vibes <= vib0) { std::printf("FAIL: hiker did not vibrate\n"); ++fails; }
  tapOn(HIT_HIDE); run(300); shot("hiker_dodged");
  // Nightcrawler pants and a real walk
  freshPet(3);
  hatchNow();
  toLocal(21, 0); run(300);
  tapOn(HIT_TAB, 1); run(200); scrollDown(2); run(100);
  shot("nc_care_bottom");
  // Nightcrawler rows: FEED TREAT PLAY HIDE CLEAN LIGHTS WALK PANTS (walk 6, pants 7).
  tapOn(HIT_ROW, 7); run(200); shot("pants_overlay");
  tapOn(HIT_PANTS, 1); run(300); shot("pants_chosen");
  // Walk with the motion sensor: no taps, just steps.
  P.hasAcc = true; P.ax = 0; P.ay = 0.3f; P.az = 0.95f;
  tapOn(HIT_TAB, 1); run(200); scrollDown(2); run(100);
  tapOn(HIT_ROW, 6); run(300);
  for (int i = 0; i < 36; ++i) {          // about 2 steps a second
    P.az = 1.35f; run(120);
    P.az = 0.95f; run(380);
  }
  shot("walk_real_steps");
  run(3000); shot("walk_result");
  if (app->pet().state().walked != 1) { std::printf("FAIL: a real walk did not count\n"); ++fails; }
  tapCenter(); run(200);
  // Lying face down from the start (wrong axes) must not nap...
  P.hasAcc = false; run(6000);            // no face-up reading for a while
  P.hasAcc = true; P.az = -1.0f; P.ay = 0;
  run(4000);
  if (!app->screenOn()) { std::printf("FAIL: face down without a flip turned the screen off\n"); ++fails; }
  // ...but turning it over from face up does.
  P.az = 1.0f; run(500);
  P.az = -1.0f;
  run(2000);
  if (app->screenOn()) { std::printf("FAIL: face down did not turn the screen off\n"); ++fails; }
  P.az = 1.0f; P.hasAcc = false;
  app->onButton(0); run(200);
  if (!app->screenOn()) { std::printf("FAIL: side key did not wake the screen\n"); ++fails; }
  // Side keys step the tabs.
  app->onButton(1); run(100); app->onButton(1); run(100); shot("key_b_twice");
  // Release an adult
  app->pet().state().form = (uint8_t)cryptlings::Form::LEGEND;
  app->pet().state().restless = 1;
  tapOn(HIT_TAB, 2); run(200);
  scrollDown(8);
  shot("file_release");
  holdOn(HIT_HOLD_RELEASE, 1700); run(300); shot("farewell");
  tapCenter(); run(500); shot("tray_after_release");

  // ---- regression checks (same as the rectangle build) ----
  {
    freshPet(1);
    hatchNow();
    app->pet().state().evidence[1][0] = 4;
    app->pet().state().form = (uint8_t)cryptlings::Form::LEGEND;
    app->forceSave();
    tapOn(HIT_TAB, 2); run(200);
    scrollDown(8);
    holdOn(HIT_HOLD_RELEASE, 1700); run(300); tapCenter(); run(300);
    newApp(true);
    if (app->pet().state().evidence[1][0] != 4) { std::printf("FAIL: evidence lost after release+reboot\n"); ++fails; }
    freshPet(0);
    hatchNow();
    for (int i = 0; i < 5; ++i) app->pet().state().stat[i] = 60;
    tapOn(HIT_TAB, 3); run(200); scrollDown(2); run(100);
    tapOn(HIT_ROW, 4); run(200);
    for (int i = 0; i < 4; ++i) tapOn(HIT_CLOCK, 4);   // +1 day x4
    tapOn(HIT_DONE); run(300);
    const cryptlings::State& cs = app->pet().state();
    if (!cs.hibernating) { std::printf("FAIL: 4 days forward did not hibernate\n"); ++fails; }
    for (int i = 0; i < 5; ++i) if (cs.stat[i] < 9.9f) { std::printf("FAIL: stat %d below floor (%.1f)\n", i, cs.stat[i]); ++fails; }
    shot("clock_forward_burrow");
  }
  freshPet(2);
  tapOn(HIT_TAB, 3); run(200); scrollDown(2); run(100); shot("more_with_saver");
  app->setPowerHold(0.55f); run(100); shot("power_hold");
  app->setPowerHold(0); app->renderPowerOff(); shot("powered_off", false);
  if (missing) { std::printf("FAIL: %d controls not found\n", missing); ++fails; }
  std::printf("regression checks: %s\n", fails ? "FAILED" : "passed");
  std::printf("wrote %d screenshots to %s (notes played: %d, vibrations: %d)\n", shotNo, outDir.c_str(), P.notes, P.vibes);
  return fails ? 1 : 0;
}
#endif  // CRYPT_ROUND
