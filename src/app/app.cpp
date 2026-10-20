// App flow: input, engine ticking, events, care actions, sound, saving.
// Drawing lives in app_draw.cpp, minigames in app_game.cpp.
#include "app.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "data.h"
#include "layout.h"

using namespace cryptlings;

namespace {
// Motif ids (Brief B10).
enum { M_HOOT = 0, M_CHIME, M_EVOLVE, M_HATCH, M_SHUTTER, M_DUSK, M_DAWN, M_BLIP, M_RELEASE };
const Note HOOT[]    = {{392, 300}, {0, 60}, {330, 450}};
const Note CHIME[]   = {{1047, 80}, {1319, 160}};
const Note EVOLVE[]  = {{523, 110}, {587, 110}, {659, 110}, {784, 110}, {880, 300}};
const Note HATCH[]   = {{1319, 40}, {0, 30}, {1319, 40}, {0, 30}, {1319, 40}, {0, 40}, {784, 200}};
const Note SHUTTER[] = {{3000, 20}, {0, 150}, {3000, 20}};
const Note DUSK[]    = {{440, 200}, {523, 200}, {659, 400}};
const Note DAWN[]    = {{659, 200}, {523, 200}, {440, 400}};
const Note BLIP[]    = {{1800, 15}};
const Note RELEASE[] = {{880, 160}, {784, 160}, {659, 160}, {587, 160}, {523, 320}};
struct M { const Note* n; int len; uint16_t vibe; };   // vibe: ms of vibration (0 none)
const M MOTIFS[] = {{HOOT, 3, 200}, {CHIME, 2, 0}, {EVOLVE, 5, 400}, {HATCH, 7, 300}, {SHUTTER, 3, 250},
                    {DUSK, 3, 0}, {DAWN, 3, 0}, {BLIP, 1, 0}, {RELEASE, 5, 0}};

const uint32_t TIMEOUTS[3] = {30000, 60000, 120000};
const uint8_t BRIGHT[3] = {70, 150, 255};
}  // namespace

// Hold targets
enum { H_NONE = 0, H_INCUBATE, H_PET, H_DIG, H_RELEASE, H_RESET };

// ---------------------------------------------------------------------------
void App::begin(uint32_t ms, const uint8_t* petData, size_t petLen, const Settings* s) {
  ms_ = ms;
  now_ = P.unixNow();
  if (s && s->magic == SETTINGS_MAGIC && s->version >= 1 && s->version <= SETTINGS_VERSION) {
    set_ = *s;
    if (set_.version < 2) { set_.saver = 1; set_.version = 2; }   // battery saver on by default
  } else {
    std::memset(&set_, 0, sizeof(set_));
    set_.magic = SETTINGS_MAGIC;
    set_.version = SETTINGS_VERSION;
    set_.saver = 1;
    set_.sound = 0;
    set_.brightness = 1;
    set_.timeout = 1;
    set_.caseNo = (uint16_t)(400 + (now_ % 600));
  }
  set_.name[sizeof(set_.name) - 1] = 0;
  // The pet blob also carries the collection (evidence, species seen), so it is
  // loaded even when the last pet was released. hasPet only picks TRAY vs DEN.
  bool loaded = petData && petLen && pet_.load(petData, petLen);
  petValid_ = loaded;
  if (loaded && !set_.hasPet) {
    // Released or reset: keep the collection, nothing to simulate.
  } else if (loaded) {
    pet_.state().utcOffsetSec = P.utcOffset(now_);
    pet_.catchUp(now_);
    Event e;
    while (pet_.nextEvent(e)) {}
  } else {
    set_.hasPet = 0;
  }
  scr_ = Scr::BOOT;
  bootStart_ = ms;
  lastInput_ = ms;
  lastTick_ = ms;
  lastSave_ = ms;
  P.backlight(BRIGHT[set_.brightness % 3]);
  dirty_ = true;
}

void App::forceSave() { save(true); }

void App::save(bool force) {
  if (!force && ms_ - lastSave_ < 30000) return;
  if (petValid_) {
    uint8_t buf[sizeof(State)];
    size_t n = pet_.serialize(buf, sizeof(buf));
    P.savePet(buf, n);
  }
  P.saveSettings(set_);
  lastSave_ = ms_;
}

void App::showToast(const char* msg, uint32_t ms) {
  std::snprintf(toast_, sizeof(toast_), "%s", msg);
  toastUntil_ = ms_ + ms;
  dirty_ = true;
}

void App::playMotif(int id, bool chime) {
  if (set_.sound == 2) return;
  if (set_.sound == 1 && !chime) return;
  bool attended = screenOn_;
  if (!attended) {
    if (set_.hasPet && pet_.inQuiet(now_)) return;          // owner is asleep
    int32_t day = (int32_t)((now_ + P.utcOffset(now_)) / 86400);
    if (day != soundDay_) { soundDay_ = day; soundsToday_ = 0; }
    if (soundsToday_ >= 6) return;
    ++soundsToday_;
  }
  P.play(MOTIFS[id].n, MOTIFS[id].len);
  if (MOTIFS[id].vibe) P.vibrate(MOTIFS[id].vibe);
}

// Motion sensor (StopWatch): read once per update.
//  - Turned face down for 1.5 s turns the screen off (outside games and the hiker alert).
//  - During a walk, real steps count alongside taps.
//  - In the Lamplight Waltz, tilting steers Mothman when no finger is down.
void App::motionTick(uint32_t dt) {
  float x, y, z;
  hasMotion_ = P.accel(x, y, z);
  if (!hasMotion_) return;
  acc_[0] = x; acc_[1] = y; acc_[2] = z;
  if (!screenOn_) { faceDownMs_ = 0; return; }

  bool canNap = scr_ != Scr::GAME && ov_ != Ov::HIKER && ov_ != Ov::REVEAL && !ptr_.down;
  // Only a real flip counts: face up within the last 5 s, then face down. If
  // the sensor's axes were ever wrong (always reading face down), it never naps.
  if (z > 0.6f) faceUpMs_ = ms_;
  bool flipped = faceDownMs_ > 0 || (faceUpMs_ && ms_ - faceUpMs_ < 5000);
  if (z < -0.75f && canNap && flipped) {
    faceDownMs_ += dt;
    if (faceDownMs_ > 1500) { faceDownMs_ = 0; sleepScreen(); return; }
  } else {
    faceDownMs_ = 0;
  }

  if (scr_ != Scr::GAME) return;
  if (g_.type == Game::STROLL) {
    // A step is a bump in total acceleration above its running average.
    float m = std::sqrt(x * x + y * y + z * z);
    accAvg_ += (m - accAvg_) * 0.05f;
    float d = m - accAvg_;
    if (!stepHigh_ && d > 0.15f && ms_ - lastStepMs_ > 300) {
      stepHigh_ = true;
      lastStepMs_ = ms_;
      ++g_.steps; ++g_.score; ++g_.realSteps;
      dirty_ = true;
    } else if (stepHigh_ && d < 0.05f) {
      stepHigh_ = false;
    }
  } else if (g_.type == Game::WALTZ && !ptr_.down) {
    const float k = 0.35f;   // px per ms per g
    g_.px -= x * k * dt;
    g_.py -= y * k * dt;
#if CRYPT_ROUND
    float dx = g_.px - L::CX, dy = g_.py - L::CY, r = std::sqrt(dx * dx + dy * dy);
    if (r > 200) { g_.px = L::CX + dx * 200 / r; g_.py = L::CY + dy * 200 / r; }
#else
    if (g_.px < 0) g_.px = 0;
    if (g_.px > gfx::W) g_.px = gfx::W;
    if (g_.py < L::G_TOP) g_.py = L::G_TOP;
    if (g_.py > gfx::H) g_.py = gfx::H;
#endif
  }
}

// ---------------------------------------------------------------------------
int App::tod() const {
  float h = const_cast<Pet&>(pet_).localHour(now_);
  if (!set_.hasPet) {
    int32_t off = const_cast<Platform&>(P).utcOffset(now_);
    int64_t l = (int64_t)now_ + off;
    h = (float)((l % 86400 + 86400) % 86400) / 3600.0f;
  }
  if (h < 5.5f) return 1;
  if (h < 7.0f) return 2;
  if (h < 18.0f) return 3;
  if (h < 19.5f) return 0;
  return 1;
}

bool App::asleepNow() const {
  if (!set_.hasPet) return false;
  if ((Form)pet_.state().form == Form::EGG) return false;
  return !pet_.awake(now_);
}

int App::formIdx() const {
  Form f = (Form)pet_.state().form;
  if (f == Form::HOAX) return 4;
  return (int)f;
}

int App::moodIdx() const {
  if (asleepNow() || pet_.state().hibernating) return 3;
  const State& s = pet_.state();
  float low = s.stat[0];
  for (int i : {1, 2, 4}) if (s.stat[i] < low) low = s.stat[i];
  if (low < 25) return 2;
  if (s.stat[(int)Stat::CHEER] > 70 && s.stat[(int)Stat::BELLY] > 50) return 0;
  return 1;
}

int App::bestAction() const {
  const State& s = pet_.state();
  Form f = (Form)s.form;
  if (f == Form::EGG) return A_WARM;
  if (asleepNow()) return A_LIGHTS;
  struct C { float v; int a; } c[] = {{s.stat[(int)Stat::BELLY], A_FEED},
                                      {s.stat[(int)Stat::REST], A_LIGHTS},
                                      {s.stat[(int)Stat::TIDY], A_CLEAN},
                                      {s.stat[(int)Stat::CHEER], A_PLAY},
                                      {s.stat[(int)Stat::MYST], A_HIDE}};
  int best = 0;
  for (int i = 1; i < 5; ++i) if (c[i].v < c[best].v) best = i;
  if (c[best].v > 75) return A_PLAY;
  return c[best].a;
}

const char* App::hintLine(char* buf, int n) {
  const State& s = pet_.state();
  Species sp = (Species)s.species;
  Form f = (Form)s.form;
  if (s.hibernating) return "Hibernating. Dig it out.";
  if (f == Form::EGG) return now_ + 120 >= s.hatchAt ? "It is wiggling. Soon." : "Hold the egg to warm it.";
  if (pet_.hikerPending(now_)) return "A hiker! HIDE, quick!";
  if (asleepNow()) {
    float h = pet_.localHour(now_);
    if (!s.tucked && h >= 5.5f && h < 8.0f) return "Dawn. Tuck it in: LIGHTS.";
    return "Asleep till dusk. Shh.";
  }
  if (s.restless) return "Something out there calls.";
  const char* lows[] = {"Hungry. FEED it.", "Bored. PLAY?", "Sleepy. A nap?", "Too visible. HIDE.", "Grubby. CLEAN it."};
  int li = 0;
  for (int i = 1; i < 5; ++i) if (s.stat[i] < s.stat[li]) li = i;
  if (s.stat[li] < 40) return lows[li];
  if (sp == Species::NESSIE && s.loch < 40) return "The loch is low. Top up.";
  if (sp == Species::NIGHTCRAWLER && !s.walked) return "He eyes the door. WALK?";
  if (sp == Species::NIGHTCRAWLER && !s.pantsDone) {
    std::snprintf(buf, n, "He eyes the %s.", PANTS[s.pantsWanted & 3]);
    return buf;
  }
  if (s.stat[li] < 60) return lows[li];
  if (pet_.localHour(now_) >= 20.0f && pet_.localHour(now_) < 23.0f && f < Form::LEGEND) return "Fog's nice. Tap to pat.";
  return "All well. Tap to pat.";
}

// ---------------------------------------------------------------------------
void App::wake() {
  screenOn_ = true;
  P.backlight(BRIGHT[set_.brightness % 3]);
  lastInput_ = ms_;
  dirty_ = true;
}

void App::sleepScreen() {
  if (lampOn_) { pet_.setLamp(now_, false); lampOn_ = false; }
  screenOn_ = false;
  sessionOpen_ = false;
  P.backlight(0);
  save(true);
}

void App::sleepNow() {
  if (screenOn_) sleepScreen();
}

void App::setPowerHold(float p) {
  if (p < 0) p = 0;
  if (p > 1) p = 1;
  if (p != powerHold_) { powerHold_ = p; dirty_ = true; }
  if (p > 0) lastInput_ = ms_;
}

void App::ensureSession() {
  if (!set_.hasPet) return;
  if (!sessionOpen_ || ms_ - lastSessionMs_ > 300000) {
    pet_.beginSession(now_);
    sessionOpen_ = true;
  }
  lastSessionMs_ = ms_;
}

void App::tickEngine(bool force) {
  if (!set_.hasPet) return;
  if (ov_ == Ov::CLOCK) return;                 // the clock is being changed: wait for DONE
  if (!force && ms_ - lastTick_ < 1000) return;
  lastTick_ = ms_;
  pet_.state().utcOffsetSec = P.utcOffset(now_);
  pet_.tick(now_);
  handleEvents();
  if (pet_.state().hibernating && ov_ == Ov::NONE && scr_ == Scr::MAIN) ov_ = Ov::BURROW;

  // Need hoot while the screen is off (Brief B10 limits).
  if (!screenOn_ && !asleepNow() && (Form)pet_.state().form != Form::EGG && !pet_.state().hibernating) {
    const State& s = pet_.state();
    float low = s.stat[0];
    for (int i : {1, 2, 4}) if (s.stat[i] < low) low = s.stat[i];
    if (low < 30 && (lastHootMs_ == 0 || ms_ - lastHootMs_ > 45UL * 60UL * 1000UL)) {
      lastHootMs_ = ms_;
      playMotif(M_HOOT, false);
    }
  }
  if (ms_ - lastSave_ > 600000) save(true);
  if (screenOn_) dirty_ = true;
}

void App::handleEvents() {
  Event e;
  char buf[120];
  const SpeciesText& T = SPECIES_TEXT[pet_.state().species & 3];
  while (pet_.nextEvent(e)) {
    switch (e.kind) {
      case EventKind::HATCHED:
        pendingReveal_ = (uint8_t)Form::WHISPER;   // shown when the screen is free
        playMotif(M_HATCH, true);
        save(true);
        break;
      case EventKind::EVOLVED:
        pendingReveal_ = e.a;
        playMotif(M_EVOLVE, true);
        save(true);
        break;
      case EventKind::EVIDENCE:
        std::snprintf(buf, sizeof(buf), "Evidence: %s. Filed as %s.", T.evidence[e.a & 15], TIER_NAME[e.b % 5]);
        showToast(buf, 3200);
        playMotif(M_CHIME, true);
        break;
      case EventKind::HIKER:
        playMotif(M_SHUTTER, false);
        hikerWaiting_ = true;   // the alert opens as soon as the screen is free (see update)
        break;
      case EventKind::HIKER_MISSED:
        showToast("Nobody hid him. A hiker got a blurry photo. Mystique down.", 3200);
        break;
      case EventKind::DIZZY:
        lampOn_ = false;
        showToast("Too much lamp. He bonked the glass and sees spirals.", 3000);
        break;
      case EventKind::MISSED_WALK:
        showToast("No walk last night. He sulked by the door.", 3000);
        break;
      case EventKind::WOKE_AT_DUSK:
        playMotif(M_DUSK, false);
        break;
      case EventKind::WENT_TO_SLEEP:
        break;
      case EventKind::RESTLESS:
        showToast("Something out there is calling. It can be released from the File.", 3600);
        break;
      case EventKind::MESS:
        if (screenOn_) showToast("Made a bit of a mess.", 1600);
        break;
      default: break;
    }
  }
}

// ---------------------------------------------------------------------------
bool App::update(uint32_t ms, const TouchIn& t) {
  ms_ = ms;
  now_ = P.unixNow();

  // ---- touch ----
  if (t.down && !ptr_.down) {
    ptr_ = Ptr();
    ptr_.down = true;
    ptr_.sx = ptr_.x = ptr_.lx = t.x;
    ptr_.sy = ptr_.y = ptr_.ly = t.y;
    ptr_.t0 = ms;
    if (!screenOn_) { wake(); ptr_.swallow = true; }
    lastInput_ = ms;
    holdId_ = H_NONE;
    if (!ptr_.swallow) {
      // What is under the finger that responds to a hold?
      switch (hitAt(t.x, t.y).id) {
        case HIT_HOLD_DIG: holdId_ = H_DIG; holdNeed_ = 1500; break;
        case HIT_HOLD_RESET: holdId_ = H_RESET; holdNeed_ = 2000; break;
        case HIT_HOLD_INCUBATE: holdId_ = H_INCUBATE; holdNeed_ = 2000; break;
        case HIT_PET: holdId_ = H_PET; holdNeed_ = 800; break;
        case HIT_HOLD_RELEASE: holdId_ = H_RELEASE; holdNeed_ = 1500; break;
        default: break;
      }
    }
    dirty_ = true;
  } else if (t.down && ptr_.down) {
    int dx = t.x - ptr_.lx, dy = t.y - ptr_.ly;
    ptr_.x = t.x; ptr_.y = t.y;
    int tx = t.x - ptr_.sx, ty = t.y - ptr_.sy;
    if (!ptr_.moved && (tx * tx + ty * ty) > 100) {
      ptr_.moved = true;
      if (holdId_ != H_PET) holdId_ = H_NONE;
    }
    if (ptr_.moved) {
      if (holdId_ == H_PET) holdId_ = H_NONE;
      int dir = dx > 2 ? 1 : (dx < -2 ? -1 : 0);
      if (dir && dir != ptr_.lastDir) { if (ptr_.lastDir) ++ptr_.reversals; ptr_.lastDir = dir; }
      if (!ptr_.swallow) onDrag(dx, dy);
    }
    ptr_.lx = t.x; ptr_.ly = t.y;
    lastInput_ = ms;
    if (holdId_ != H_NONE && !ptr_.swallow) {
      holdP_ = (float)(ms - ptr_.t0) / (float)holdNeed_;
      if (holdP_ >= 1.0f) {
        int id = holdId_;
        holdId_ = H_NONE;
        holdP_ = 0;
        ptr_.swallow = true;
        onHoldDone(id);
      }
      dirty_ = true;
    }
  } else if (!t.down && ptr_.down) {
    ptr_.down = false;
    if (!ptr_.swallow) {
      int dx = ptr_.x - ptr_.sx, dy = ptr_.y - ptr_.sy;
      bool startedOnPet = hitAt(ptr_.sx, ptr_.sy).id == HIT_PET;
      if (!ptr_.moved && ms - ptr_.t0 < 600) onTap(ptr_.sx, ptr_.sy);
      else if (ptr_.moved && startedOnPet && ptr_.reversals >= 3) act(A_SCRUB);
      else if (ptr_.moved && (dx > 40 || dx < -40) && (dx > 0 ? dx : -dx) > 2 * (dy > 0 ? dy : -dy))
        onSwipe(dx < 0 ? -1 : 1);
    }
    holdId_ = H_NONE;
    holdP_ = 0;
    dirty_ = true;
  }

  // ---- motion (no-op without a sensor) ----
  {
    uint32_t dt = lastUpdateMs_ ? ms - lastUpdateMs_ : 0;
    lastUpdateMs_ = ms;
    if (dt > 200) dt = 200;
    bool wasOn = screenOn_;
    motionTick(dt);
    if (wasOn && !screenOn_) return false;   // laid face down: lights out this frame
  }

  // ---- game ----
  if (scr_ == Scr::GAME) { updateGame(); dirty_ = true; lastInput_ = ms; }

  // ---- engine ----
  tickEngine();

  // ---- screen timeout ----
  if (screenOn_ && scr_ != Scr::GAME && ov_ != Ov::HIKER && !ptr_.down &&
      ms - lastInput_ > TIMEOUTS[set_.timeout % 3]) {
    sleepScreen();
    return false;
  }

  // ---- deferred overlays (hiker first: it is time critical) ----
  if (screenOn_ && scr_ == Scr::MAIN && ov_ == Ov::NONE && !ptr_.down) {
    if (hikerWaiting_ && pet_.hikerPending(now_)) {
      ov_ = Ov::HIKER; hikerShownMs_ = ms; ovSince_ = ms; hikerWaiting_ = false; dirty_ = true;
    } else if (pendingReveal_ != 0xFF) {
      revealForm_ = pendingReveal_; revealStart_ = ms; ov_ = Ov::REVEAL; ovSince_ = ms;
      pendingReveal_ = 0xFF; dirty_ = true;
    }
    if (hikerWaiting_ && !pet_.hikerPending(now_)) hikerWaiting_ = false;
  }

  // ---- hiker alert timer (10 s on screen) ----
  if (ov_ == Ov::HIKER) {
    if (!pet_.hikerPending(now_)) ov_ = Ov::NONE;
    else if (ms - hikerShownMs_ > 10000) {
      pet_.resolveHiker(now_, false);
      ov_ = Ov::NONE;
      showToast("Too slow. Snap! A blurry photo. Mystique down.", 3000);
      handleEvents();
    }
    dirty_ = true;
  }

  // ---- animation cadence ----
  if (screenOn_ && ms - lastAnim_ >= 90) {
    bool animated = scr_ == Scr::BOOT || scr_ == Scr::TRAY || scr_ == Scr::NAME ||
                    (scr_ == Scr::MAIN && tab_ == Tab::DEN) || ov_ == Ov::REVEAL || ov_ == Ov::BURROW;
    if (animated) { lastAnim_ = ms; dirty_ = true; }
  }
  if (toastUntil_ && ms > toastUntil_) { toastUntil_ = 0; toast_[0] = 0; dirty_ = true; }
  if (scr_ == Scr::BOOT && ms - bootStart_ > 9000) { scr_ = set_.hasPet ? Scr::MAIN : Scr::TRAY; dirty_ = true; }

  bool d = dirty_ && screenOn_;
  dirty_ = false;
  return d;
}

// ---------------------------------------------------------------------------
void App::onSwipe(int dir) {
  if (ov_ != Ov::NONE) return;
  if (scr_ == Scr::TRAY) { trayPick_ = (trayPick_ - dir + 4) % 4; trayScroll_ = 0; return; }
  if (scr_ == Scr::NAME) { nameIdx_ = (nameIdx_ + 1) % 6; return; }
  if (scr_ != Scr::MAIN) return;
  int t = (int)tab_ - dir;
  if (t < 0 || t > 3) return;
  tab_ = (Tab)t;
}

void App::onDrag(int dx, int dy) {
  (void)dx;
  if (ov_ != Ov::NONE) return;
  if (scr_ == Scr::TRAY && hitAt(ptr_.sx, ptr_.sy).id == HIT_TRAY_TEXT) {
    trayScroll_ -= dy;
    if (trayScroll_ < 0) trayScroll_ = 0;
    if (trayScroll_ > 400) trayScroll_ = 400;
    dirty_ = true;
    return;
  }
  if (scr_ != Scr::MAIN || tab_ == Tab::DEN) return;
  int i = (int)tab_;
  scroll_[i] -= dy;
  int maxS = contentH_[i] - L::VIEW_H;
  if (maxS < 0) maxS = 0;
  if (scroll_[i] > maxS) scroll_[i] = maxS;
  if (scroll_[i] < 0) scroll_[i] = 0;
  dirty_ = true;
}

void App::onHoldDone(int id) {
  switch (id) {
    case H_INCUBATE: newPet(trayPick_); break;
    case H_PET: ensureSession(); act((Form)pet_.state().form == Form::EGG ? A_WARM : A_CUDDLE); break;
    case H_DIG:
      pet_.digOut(now_);
      ov_ = Ov::NONE;
      showToast("Dug out of the burrow. Sleepy, a bit muddy, very glad to see you.");
      save(true);
      break;
    case H_RELEASE: {
      Form f = pet_.release(now_);
      const SpeciesText& T = SPECIES_TEXT[pet_.state().species & 3];
      std::snprintf(farewell_, sizeof(farewell_), "%s walks into the fog as a %s. It left you %s.",
                    set_.name, STAGE_NAME[(int)f], T.souvenir);
      ov_ = Ov::FAREWELL;
      playMotif(M_RELEASE, true);
      set_.hasPet = 0;
      save(true);
      break;
    }
    case H_RESET:
      set_.hasPet = 0;
      ov_ = Ov::NONE;
      scr_ = Scr::TRAY;
      save(true);
      break;
  }
}

void App::newPet(int species) {
  bool first = pet_.state().raisedMask == 0;
  pet_.spawnEgg((Species)species, now_, P.utcOffset(now_), first);
  petValid_ = true;
  set_.hasPet = 1;
  for (int& s : scroll_) s = 0;
  trayScroll_ = 0;
  lampOn_ = false;
  pendingReveal_ = 0xFF;
  hikerWaiting_ = false;
  tab_ = Tab::DEN;
  nameIdx_ = (int)(now_ % 6);
  scr_ = Scr::NAME;
  playMotif(M_HATCH, true);
}

// ---------------------------------------------------------------------------
void App::onTap(int x, int y) {
  if (set_.sound == 0 && screenOn_) P.play(MOTIFS[M_BLIP].n, 1);
  const Hit h = hitAt(x, y);

  // Overlays first.
  if (ov_ != Ov::NONE) {
    switch (ov_) {
      case Ov::REVEAL:
        if (ms_ - revealStart_ > 3000) ov_ = Ov::NONE;
        break;
      case Ov::HIKER:
        if (h.id == HIT_HIDE) {
          ensureSession();
          pet_.hide(now_);   // resolves the pending hiker as a dodge...
          if (pet_.hikerPending(now_)) pet_.resolveHiker(now_, true);   // ...even if HIDE is on cooldown
          ov_ = Ov::NONE;
          showToast("Hid behind a tree just in time. The hiker photographed a stump.");
          handleEvents();
          save();
        }
        break;
      case Ov::FAREWELL:
        ov_ = Ov::NONE;
        scr_ = Scr::TRAY;
        tab_ = Tab::DEN;
        break;
      case Ov::PANTS:
        if (h.id == HIT_PANTS) {
          int i = h.idx;
          ensureSession();
          const State& s = pet_.state();
          bool right = i == s.pantsWanted;
          Result r = pet_.wearPants(now_, (uint8_t)i);
          ov_ = Ov::NONE;
          if (r == Result::COOLDOWN) showToast("One outfit per night. He is firm about this.");
          else showToast(right ? "The right pants. He does a tiny, dignified twirl." : "Wrong pants. He wears them anyway, quietly disappointed.");
        }
        if (h.id == HIT_CANCEL) ov_ = Ov::NONE;
        break;
      case Ov::CLOCK: {
        static const int32_t steps[6] = {3600, -3600, 600, -600, 86400, -86400};
        if (h.id == HIT_CLOCK) { P.setClock((uint32_t)((int64_t)now_ + steps[h.idx % 6])); now_ = P.unixNow(); }
        if (h.id == HIT_DONE) {
          ov_ = Ov::NONE;
          // A clock moved forward is time away, not live play: capped catch-up.
          if (set_.hasPet && now_ > pet_.state().lastSeen + 300) {
            pet_.state().utcOffsetSec = P.utcOffset(now_);
            pet_.catchUp(now_);
            handleEvents();
          }
          tickEngine(true);
          save(true);
        }
        break;
      }
      case Ov::RESULT:
        if (ms_ - ovSince_ > 400) ov_ = Ov::NONE;
        break;
      case Ov::CONFIRM_RESET:
        if (h.id == HIT_CANCEL) ov_ = Ov::NONE;
        break;
      default: break;
    }
    return;
  }

  switch (scr_) {
    case Scr::BOOT:
      scr_ = set_.hasPet ? Scr::MAIN : Scr::TRAY;
      if (!set_.bootSeen) { set_.bootSeen = 1; P.saveSettings(set_); }
      return;
    case Scr::TRAY:
      if (h.id == HIT_PREV) { trayPick_ = (trayPick_ + 3) % 4; trayScroll_ = 0; }
      else if (h.id == HIT_NEXT) { trayPick_ = (trayPick_ + 1) % 4; trayScroll_ = 0; }
      return;
    case Scr::NAME:
      if (h.id == HIT_ANOTHER) nameIdx_ = (nameIdx_ + 1) % 6;
      if (h.id == HIT_USE) {
        std::snprintf(set_.name, sizeof(set_.name), "%s", SPECIES_TEXT[pet_.state().species & 3].names[nameIdx_]);
        scr_ = Scr::MAIN;
        tab_ = Tab::DEN;
        save(true);
        showToast("Egg filed. Keep it warm: hold the egg.", 3200);
      }
      return;
    case Scr::GAME:
      return;   // the game reads the pointer directly
    case Scr::MAIN:
      break;
  }

  switch (h.id) {
    case HIT_TAB: tab_ = (Tab)h.idx; return;
    case HIT_PET:
      ensureSession();
      act((Form)pet_.state().form == Form::EGG ? A_WARM : A_PAT);
      return;
    case HIT_BEST: act(bestAction()); return;
    case HIT_CARE: tab_ = Tab::CARE; return;
    case HIT_ROW: break;
    default: return;
  }
  if (tab_ == Tab::CARE) {
    int ids[12];
    int n = careRows(ids, 12);
    if (h.idx < n) act(ids[h.idx]);
    return;
  }
  if (tab_ == Tab::MORE) {
    int i = h.idx;
    if (i == 0) set_.sound = (uint8_t)((set_.sound + 1) % 3);
    if (i == 1) { set_.brightness = (uint8_t)((set_.brightness + 1) % 3); P.backlight(BRIGHT[set_.brightness]); }
    if (i == 2) set_.timeout = (uint8_t)((set_.timeout + 1) % 3);
    if (i == 3) {
      set_.saver = set_.saver ? 0 : 1;
      showToast(set_.saver ? "Battery saver on. It rests deeply while the screen is off."
                           : "Battery saver off. Faster to wake, uses more battery.");
    }
    if (i == 4) ov_ = Ov::CLOCK;
    if (i == 5) ov_ = Ov::CONFIRM_RESET;
    P.saveSettings(set_);
  }
}

// Side keys (StopWatch): step through the tabs, or close what is open.
void App::onButton(int b) {
  lastInput_ = ms_;
  if (!screenOn_) { wake(); return; }
  dirty_ = true;
  if (scr_ == Scr::BOOT) { scr_ = set_.hasPet ? Scr::MAIN : Scr::TRAY; return; }
  if (scr_ == Scr::TRAY && ov_ == Ov::NONE) { trayPick_ = (trayPick_ + (b ? 1 : 3)) % 4; trayScroll_ = 0; return; }
  if (scr_ == Scr::NAME) { if (b) nameIdx_ = (nameIdx_ + 1) % 6; return; }
  if (scr_ == Scr::GAME) return;
  if (ov_ == Ov::PANTS || ov_ == Ov::CONFIRM_RESET) { ov_ = Ov::NONE; return; }
  if (ov_ == Ov::RESULT && ms_ - ovSince_ > 400) { ov_ = Ov::NONE; return; }
  if (ov_ == Ov::REVEAL && ms_ - revealStart_ > 3000) { ov_ = Ov::NONE; return; }
  if (ov_ != Ov::NONE) return;
  int t = (int)tab_ + (b ? 1 : -1);
  tab_ = (Tab)((t + 4) % 4);
}

// ---------------------------------------------------------------------------
int App::careRows(int* ids, int max) const {
  int n = 0;
  auto add = [&](int a) { if (n < max) ids[n++] = a; };
  add(A_FEED); add(A_TREAT); add(A_PLAY); add(A_HIDE); add(A_CLEAN); add(A_LIGHTS);
  switch ((Species)pet_.state().species) {
    case Species::MOTHMAN: add(A_LAMP); break;
    case Species::BIGFOOT: add(A_FORAGE); break;
    case Species::NESSIE: add(A_LOCH); break;
    case Species::NIGHTCRAWLER: add(A_WALK); add(A_PANTS); break;
    default: break;
  }
  return n;
}

void App::resultToast(Result r, const char* okMsg) {
  char buf[120];
  switch (r) {
    case Result::OK: showToast(okMsg); break;
    case Result::WOKE_IT:
      std::snprintf(buf, sizeof(buf), "You woke %s in the daytime. Grumpy. %s", set_.name, okMsg);
      showToast(buf, 3200);
      break;
    case Result::COOLDOWN: showToast("Not yet. Give it a little while."); break;
    case Result::NOT_NOW: showToast("Not right now."); break;
    case Result::HIBERNATING: ov_ = Ov::BURROW; break;
  }
}

void App::act(int a) {
  if (!set_.hasPet) return;
  ensureSession();
  const State& s = pet_.state();
  const SpeciesText& T = SPECIES_TEXT[s.species & 3];
  Species sp = (Species)s.species;
  if (s.hibernating) { ov_ = Ov::BURROW; return; }
  if ((Form)s.form == Form::EGG && a != A_WARM && a != A_PAT) {
    if (a == A_LIGHTS) { showToast("Eggs do not need a bedtime. Just warmth."); return; }
    showToast("It is still an egg. Hold it to keep it warm.");
    return;
  }
  char buf[120];
  tab_ = (a == A_PANTS || a == A_PLAY || a == A_WALK) ? tab_ : Tab::DEN;
  switch (a) {
    case A_WARM:
      pet_.warmEgg(now_);
      showToast(now_ + 120 >= s.hatchAt ? "Warm. Something inside is tapping back." : "Warm. The egg wobbles happily.");
      break;
    case A_PAT: {
      bool awake = pet_.awake(now_);
      Result r = pet_.pat(now_);
      if (!awake) showToast("It smiles in its sleep.");
      else if (r == Result::COOLDOWN) showToast("Plenty of pats this hour. It is pleased.");
      break;
    }
    case A_CUDDLE: {
      Result r = pet_.cuddle(now_);
      if (r == Result::OK) showToast(asleepNow() ? "Tucked in. Do not tell anyone where it sleeps." : "A long cuddle. Cheer up.");
      else resultToast(r, "");
      break;
    }
    case A_FEED:
      std::snprintf(buf, sizeof(buf), "%s. %s eats with great dignity.", T.food, set_.name);
      resultToast(pet_.feed(now_, Food::STAPLE), buf);
      break;
    case A_TREAT: {
      if (s.treatsLeft == 0) { showToast("No treats left tonight. More at dusk."); break; }
      std::snprintf(buf, sizeof(buf), "%s! Delighted. Do not make a habit of it.", T.treat);
      resultToast(pet_.feed(now_, Food::TREAT), buf);
      break;
    }
    case A_FORAGE:
      resultToast(pet_.feed(now_, Food::FORAGE), "Foraged a pawful of berries and one mushroom of questionable origin.");
      break;
    case A_PLAY:
      if (now_ < s.dizzyUntil) { showToast("Still seeing spirals. Play later."); break; }
      g_ = GameState();
      g_.type = (Game)(int)sp;
      startGame();
      break;
    case A_WALK:
      if (s.walked) { showToast("Already walked tonight. He is content."); break; }
      g_ = GameState();
      g_.type = Game::STROLL;
      g_.walk = true;
      startGame();
      break;
    case A_HIDE: {
      bool hiker = pet_.hikerPending(now_);
      Result r = pet_.hide(now_);
      if (hiker) {
        if (pet_.hikerPending(now_)) pet_.resolveHiker(now_, true);
        ov_ = Ov::NONE;
        showToast("Hid just in time. The hiker photographed a stump.");
        break;
      }
      resultToast(r, T.hideLine);
      break;
    }
    case A_CLEAN:
      resultToast(pet_.clean(now_), "Scrubbed. Smells faintly of pine and mystery.");
      break;
    case A_SCRUB:
      resultToast(pet_.scrub(now_), "Scrub scrub. It giggles.");
      break;
    case A_LIGHTS: {
      float h = pet_.localHour(now_);
      bool dawn = h >= 5.5f && h < 18.0f;
      Result r = pet_.lights(now_);
      if (r == Result::OK) showToast(dawn ? "Lights out. Tucked in until dusk." : "A short nap. Back up soon.");
      else if (r == Result::COOLDOWN) showToast(dawn ? "Already tucked in." : "One nap a night.");
      else resultToast(r, "");
      break;
    }
    case A_LAMP:
      if (!lampOn_) {
        Result r = pet_.setLamp(now_, true);
        if (r == Result::OK) { lampOn_ = true; showToast("Lamp on. He is transfixed. Do not leave it on too long."); }
        else showToast("Still dizzy. The lamp can wait.");
      } else {
        pet_.setLamp(now_, false);
        lampOn_ = false;
        showToast("Lamp off. He blinks a lot.");
      }
      break;
    case A_LOCH:
      resultToast(pet_.topUpLoch(now_), "Topped up with a bucket of rain. She does a slow, happy circle.");
      break;
    case A_PANTS:
      if (s.pantsDone) { showToast("One outfit per night. He is firm about this."); break; }
      ov_ = Ov::PANTS;
      break;
    default: break;
  }
  handleEvents();
  save();
  dirty_ = true;
}
