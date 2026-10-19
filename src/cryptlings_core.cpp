// CRYPTLINGS core engine implementation. See cryptlings_core.h.
#include "cryptlings_core.h"

#include <cmath>
#include <cstring>
#include <cstddef>
#include <initializer_list>

namespace cryptlings {

namespace {
const uint32_t MOON_REF = 947182440u;          // new moon 2000-01-06 18:14 UTC
const double   SYNODIC  = 29.530588853;        // days
const float    CHECKIN_CHANCE = 0.5f;          // TUNABLE: night check-in roll
const float    LOCH_PEBBLE_CHANCE = 0.15f;     // TUNABLE
const float    WALK_SOCK_CHANCE = 0.20f;       // TUNABLE
const float    HIDE_COVER_MIN = 30.0f;         // TUNABLE: a recent HIDE dodges hikers
const float    STREAK_CARE = 60.0f;            // TUNABLE
const uint8_t  STREAK_NIGHTS = 3;

inline float clamp100(float v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
inline int64_t localSec(uint32_t now, int32_t off) { return (int64_t)now + off; }
inline int32_t floorDiv(int64_t a, int64_t b) { return (int32_t)(a >= 0 ? a / b : -((-a + b - 1) / b)); }
}  // namespace

// ---- names ----------------------------------------------------------------
const char* formName(Form f) {
  switch (f) {
    case Form::EGG: return "EGG";
    case Form::WHISPER: return "WHISPER";
    case Form::RUMOR: return "RUMOR";
    case Form::SIGHTING: return "SIGHTING";
    case Form::LEGEND: return "LEGEND";
    case Form::BLURRY: return "BLURRY PHOTO";
    case Form::HOAX: return "HOAX";
  }
  return "?";
}
const char* speciesName(Species s) {
  switch (s) {
    case Species::MOTHMAN: return "Mothman";
    case Species::BIGFOOT: return "Bigfoot";
    case Species::NESSIE: return "Nessie";
    case Species::NIGHTCRAWLER: return "Fresno Nightcrawler";
    default: return "?";
  }
}

// ---- construction -----------------------------------------------------------
Pet::Pet(const Tuning& t) : t_(t) {
  std::memset(&s_, 0, sizeof(s_));
  s_.magic = CRYPTLINGS_MAGIC;
  s_.version = CRYPTLINGS_VERSION;
  s_.rng = 0x9E3779B9u;
}

void Pet::spawnEgg(Species sp, uint32_t now, int32_t utcOffsetSec, bool firstPet) {
  // Keep the collection across pets, reset everything else.
  uint8_t ev[(int)Species::COUNT][EVIDENCE_ITEMS];
  std::memcpy(ev, s_.evidence, sizeof(ev));
  uint8_t raised = s_.raisedMask;
  uint8_t last[(int)Species::COUNT];
  std::memcpy(last, s_.lastOutcome, sizeof(last));
  uint32_t seed = s_.rng ^ (now * 2654435761u) ^ ((uint32_t)sp << 24);
  if (!seed) seed = 1;

  std::memset(&s_, 0, sizeof(s_));
  s_.magic = CRYPTLINGS_MAGIC;
  s_.version = CRYPTLINGS_VERSION;
  std::memcpy(s_.evidence, ev, sizeof(ev));
  s_.raisedMask = raised;
  std::memcpy(s_.lastOutcome, last, sizeof(last));
  s_.rng = seed;

  s_.species = (uint8_t)sp;
  s_.form = (uint8_t)Form::EGG;
  s_.utcOffsetSec = utcOffsetSec;
  s_.firstPet = firstPet ? 1 : 0;
  s_.stat[(int)Stat::BELLY] = 80;
  s_.stat[(int)Stat::CHEER] = 70;
  s_.stat[(int)Stat::REST]  = 80;
  s_.stat[(int)Stat::MYST]  = 70;
  s_.stat[(int)Stat::TIDY]  = 90;
  s_.loch = 100;
  s_.lastSeen = now;
  s_.stageStart = now;
  s_.hatchAt = now + (uint32_t)(firstPet ? t_.eggFirstMin * 60.0f : t_.eggHours * 3600.0f);
  s_.nightId = nightOf(now);
  s_.hourId = floorDiv(localSec(now, utcOffsetSec), 3600);
  s_.dayId = floorDiv(localSec(now, utcOffsetSec), 86400);
  s_.treatsLeft = t_.treatsPerNight;
  s_.walked = 1;               // no missed-walk penalty on the first night
  s_.pantsWanted = (uint8_t)(rand32() % 4);
  s_.lastStreakNight = s_.nightId;
  s_.stagePhotos = 1u << (int)Form::EGG;
  qn_ = 0;
}

// ---- time helpers -----------------------------------------------------------
float Pet::localHour(uint32_t now) const {
  int64_t l = localSec(now, s_.utcOffsetSec);
  int64_t d = l % 86400; if (d < 0) d += 86400;
  return (float)d / 3600.0f;
}
int32_t Pet::nightOf(uint32_t now) const {
  return floorDiv(localSec(now, s_.utcOffsetSec) - 12 * 3600, 86400);
}
float Pet::moonPhase(uint32_t now) {
  double days = ((double)now - (double)MOON_REF) / 86400.0;
  double p = std::fmod(days / SYNODIC, 1.0);
  if (p < 0) p += 1.0;
  return (float)p;
}
bool Pet::nearFullMoon(uint32_t now, float windowHours) {
  float p = moonPhase(now);
  return std::fabs(p - 0.5f) * (float)SYNODIC * 24.0f <= windowHours;
}

bool Pet::awake(uint32_t now) const {
  if (now < s_.wakeUntil) return true;
  float h = localHour(now);
  float dawnStart = t_.bedHour - 1.5f;                       // tuck-in time
  if (s_.tucked && h >= dawnStart && h < t_.wakeHour) return false;
  float sleepFrom = t_.bedHour + (s_.tucked ? 0.0f : t_.untuckedGrace);
  return !(h >= sleepFrom && h < t_.wakeHour);
}
bool Pet::inQuiet(uint32_t now) const {
  float h = localHour(now);
  return t_.quietStart > t_.quietEnd ? (h >= t_.quietStart || h < t_.quietEnd)
                                     : (h >= t_.quietStart && h < t_.quietEnd);
}
static bool isNightHour(float h, const Tuning& t) { return h >= t.wakeHour || h < t.bedHour; }

// ---- rng / events -----------------------------------------------------------
uint32_t Pet::rand32() {
  uint32_t x = s_.rng ? s_.rng : 1u;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  s_.rng = x;
  return x;
}
float Pet::rand01() { return (rand32() >> 8) * (1.0f / 16777216.0f); }

void Pet::push(EventKind k, uint8_t a, uint8_t b) {
  if (qn_ == 8) { std::memmove(q_, q_ + 1, sizeof(Event) * 7); qn_ = 7; }
  q_[qn_++] = Event{k, a, b};
}
bool Pet::nextEvent(Event& out) {
  if (!qn_) return false;
  out = q_[0];
  std::memmove(q_, q_ + 1, sizeof(Event) * (qn_ - 1));
  --qn_;
  return true;
}

// ---- stats ------------------------------------------------------------------
void Pet::add(Stat s, float v) { s_.stat[(int)s] = clamp100(s_.stat[(int)s] + v); }
float Pet::careNow() const {
  return (s_.stat[(int)Stat::BELLY] + s_.stat[(int)Stat::CHEER] +
          s_.stat[(int)Stat::REST] + s_.stat[(int)Stat::TIDY]) * 0.25f;
}
float Pet::growth() const {
  return (Species)s_.species == Species::NESSIE ? t_.nessieGrowth : 1.0f;
}
float Pet::stageHoursNeeded(Form f) const {
  float base = f == Form::WHISPER ? t_.whisperHours
             : f == Form::RUMOR   ? t_.rumorHours
             : f == Form::SIGHTING? t_.sightingHours : 0.0f;
  return base / growth();
}

// ---- evidence (B8) ------------------------------------------------------------
// Items per species: 0..5 Hearsay, 6..9 Anecdote, 10..12 Eyewitness,
// 13..14 Photographic, 15 Irrefutable.
Tier Pet::evidenceTier(int i) {
  if (i <= 5) return Tier::HEARSAY;
  if (i <= 9) return Tier::ANECDOTE;
  if (i <= 12) return Tier::EYEWITNESS;
  if (i <= 14) return Tier::PHOTOGRAPHIC;
  return Tier::IRREFUTABLE;
}
int Pet::rollEvidence(int tierShift, int minTier) {
  static const float cum[5] = {0.55f, 0.80f, 0.93f, 0.99f, 1.0f};
  float r = rand01();
  int tier = 0;
  while (tier < 4 && r >= cum[tier]) ++tier;
  tier += tierShift;
  if (tier < minTier) tier = minTier;
  if (tier > 4) tier = 4;
  static const int lo[5] = {0, 6, 10, 13, 15}, n[5] = {6, 4, 3, 2, 1};
  int item = lo[tier] + (int)(rand32() % (uint32_t)n[tier]);
  uint8_t& c = s_.evidence[s_.species][item];
  if (c < 255) ++c;
  push(EventKind::EVIDENCE, (uint8_t)item, (uint8_t)tier);
  return item;
}

// ---- period rollover ------------------------------------------------------------
void Pet::rollover(uint32_t now) {
  int64_t l = localSec(now, s_.utcOffsetSec);
  int32_t hour = floorDiv(l, 3600), day = floorDiv(l, 86400), night = nightOf(now);
  // Forward only: an earlier timestamp (clock set back, DST, out-of-order
  // calls) must never reset tonight's counters a second time.
  if (hour > s_.hourId) { s_.hourId = hour; s_.patThisHour = 0; s_.sessionsThisHour = 0; }
  if (day > s_.dayId) { s_.dayId = day; s_.exposureToday = 0; }
  if (night <= s_.nightId) return;

  // A new night begins (at local noon, before dusk).
  bool live = s_.form != (uint8_t)Form::EGG && !s_.hibernating;
  Species sp = (Species)s_.species;
  if (live && sp == Species::NIGHTCRAWLER && !s_.walked) {
    add(Stat::CHEER, -t_.missedWalkCheer);
    push(EventKind::MISSED_WALK);
  }
  // Care streak from last night.
  if (live && s_.nightCareHours > 0.5f) {
    float avg = s_.nightCareSum / s_.nightCareHours;
    s_.careStreak = avg > STREAK_CARE ? (uint8_t)(s_.careStreak + 1) : 0;
    if (s_.careStreak >= STREAK_NIGHTS) { rollEvidence(0, (int)Tier::EYEWITNESS); s_.careStreak = 0; }
  } else if (live) {
    s_.careStreak = 0;
  }
  s_.nightId = night;
  s_.nightCareSum = s_.nightCareHours = 0;
  s_.playsTonight = 0;
  s_.actionRolls = 0;
  s_.treatsLeft = t_.treatsPerNight;
  s_.napUsed = 0;
  s_.checkedIn = 0;
  s_.tucked = 0;
  s_.walked = 0;
  s_.pantsDone = 0;
  s_.pantsWanted = (uint8_t)(rand32() % 4);
  s_.hikerAt[0] = s_.hikerAt[1] = 0;
  if (live && sp == Species::BIGFOOT) {
    int64_t noonLocal = (int64_t)night * 86400 + 12 * 3600;
    uint32_t base = (uint32_t)(noonLocal - s_.utcOffsetSec + (int64_t)((t_.wakeHour - 12.0f) * 3600));
    float span = (t_.quietStart - t_.wakeHour) * 3600.0f;
    int count = 1 + (int)(rand32() % 2);
    for (int i = 0; i < count; ++i) s_.hikerAt[i] = base + (uint32_t)(rand01() * span);
  }
}

// ---- simulation ---------------------------------------------------------------
void Pet::step(uint32_t t0, uint32_t t1, bool catchingUp) {
  if (t1 <= t0) return;
  if (s_.hibernating || s_.form == (uint8_t)Form::EGG) return;
  float dt = (float)(t1 - t0) / 3600.0f;
  uint32_t mid = t0 + (t1 - t0) / 2;
  bool aw = awake(mid);
  float h = localHour(mid);
  bool night = isNightHour(h, t_);
  Species sp = (Species)s_.species;

  float neg = catchingUp ? t_.catchUpDecayMul : 1.0f;
  if (aw && inQuiet(mid)) neg *= 0.5f;

  float old[(int)Stat::COUNT];
  std::memcpy(old, s_.stat, sizeof(old));

  float bellyMul = sp == Species::BIGFOOT ? t_.bigfootBellyMul : 1.0f;
  float lowLoch = 1.0f;
  if (sp == Species::NESSIE) {
    s_.loch = clamp100(s_.loch - t_.lochDrain * neg * dt);
    if (s_.loch < t_.lochLow) lowLoch = t_.lochLowMul;
  }

  add(Stat::BELLY, -(aw ? t_.bellyAwake : t_.bellyAsleep) * bellyMul * neg * dt);
  add(Stat::CHEER, -(aw ? t_.cheerAwake : t_.cheerAsleep) * lowLoch * neg * dt);
  if (aw) add(Stat::REST, -t_.restAwake * lowLoch * neg * dt);
  else    add(Stat::REST, t_.restAsleepGain * (s_.tucked ? 1.0f : t_.untuckedRestMul) * dt);
  add(Stat::TIDY, -(aw ? t_.tidyAwake : t_.tidyAsleep) * neg * dt);
  add(Stat::MYST, -t_.mystBase * neg * dt);
  if (aw && night && s_.sessionsThisHour <= 1) add(Stat::MYST, t_.quietNightMyst * dt);

  // Mothman lamp: cheer while on, dizzy if left on too long.
  if (sp == Species::MOTHMAN && s_.lampOnSince) {
    if (aw) add(Stat::CHEER, t_.lampCheer * dt);
    uint32_t dizzyAt = s_.lampOnSince + (uint32_t)(t_.lampDizzyAfterMin * 60.0f);
    if (t1 >= dizzyAt) {
      s_.lampOnSince = 0;
      s_.dizzyUntil = dizzyAt + (uint32_t)(t_.dizzyMinutes * 60.0f);
      add(Stat::CHEER, -t_.dizzyCheer);
      push(EventKind::DIZZY);
    }
  }

  if (catchingUp) {  // floor: catch-up never drags a stat below the floor
    for (int i = 0; i < (int)Stat::COUNT; ++i)
      if (s_.stat[i] < t_.catchUpFloor && old[i] > s_.stat[i])
        s_.stat[i] = old[i] < t_.catchUpFloor ? old[i] : t_.catchUpFloor;
  }

  // Aggregates.
  float care = careNow();
  Form f = (Form)s_.form;
  if (f >= Form::WHISPER && f <= Form::SIGHTING) {
    float w = (float)f;  // WHISPER 1, RUMOR 2, SIGHTING 3
    s_.careW += care * w * dt;
    s_.mystW += s_.stat[(int)Stat::MYST] * w * dt;
    s_.weight += w * dt;
    s_.stageCare += care * dt;
    s_.stageHours += dt;
    float low = s_.stat[0];
    for (int i : {(int)Stat::CHEER, (int)Stat::REST, (int)Stat::TIDY}) if (s_.stat[i] < low) low = s_.stat[i];
    // Mystique is left out on purpose: it is hidden-ness, not comfort, and it
    // already has its own 30% share of the fork score.
    if (low < t_.neglectStat) s_.neglectHours += dt * (catchingUp ? 0.5f : 1.0f);
  } else if (f >= Form::LEGEND) {
    s_.adultHours += dt;
  }
  if (aw && night) { s_.nightCareSum += care * dt; s_.nightCareHours += dt; }
}

void Pet::tick(uint32_t now) {
  if (now < s_.lastSeen) {               // clock went back: never rewind, never fast-forward
    s_.lastSeen = now;
    if (s_.stageStart > now) s_.stageStart = now;
    uint32_t* until[] = {&s_.cuddleReadyAt, &s_.hideReadyAt, &s_.scrubReadyAt, &s_.dizzyUntil, &s_.wakeUntil};
    for (uint32_t* u : until) if (*u > now + 3 * 3600) *u = now;
    if (s_.lampOnSince > now) s_.lampOnSince = now;
    return;
  }
  if (s_.hibernating) {                  // asleep in the burrow: age stands still
    s_.stageStart += now - s_.lastSeen;
    s_.hatchAt += now - s_.lastSeen;
    s_.lastSeen = now;
    rollover(now);
    return;
  }
  bool wasAwake = awake(s_.lastSeen);
  uint32_t t = s_.lastSeen;
  while (t < now) {
    uint32_t t2 = now - t > 900 ? t + 900 : now;
    rollover(t);
    step(t, t2, false);
    t = t2;
  }
  s_.lastSeen = now;
  rollover(now);
  if (s_.hibernating) return;

  bool isAwake = awake(now);
  if (s_.form != (uint8_t)Form::EGG && wasAwake != isAwake)
    push(isAwake ? EventKind::WOKE_AT_DUSK : EventKind::WENT_TO_SLEEP);

  // Bigfoot hikers: due -> alert; unanswered for too long -> missed.
  if ((Species)s_.species == Species::BIGFOOT && s_.form != (uint8_t)Form::EGG) {
    for (int i = 0; i < 2; ++i) {
      uint32_t at = s_.hikerAt[i];
      if (!at || now < at) continue;
      uint32_t hideStart = s_.hideReadyAt ? s_.hideReadyAt - (uint32_t)(t_.hideCooldownMin * 60.0f) : 0;
      bool covered = s_.hideReadyAt && now - hideStart <= (uint32_t)(HIDE_COVER_MIN * 60.0f);
      if (covered) {                       // already hiding: a free dodge
        add(Stat::MYST, t_.hikerDodge);
        s_.hikerAt[i] = 0;
      } else if (!s_.hikerShownAt) {
        s_.hikerShownAt = at;
        s_.hikerShownSince = now;
        push(EventKind::HIKER);
      } else if (s_.hikerShownAt == at && now - s_.hikerShownSince > (uint32_t)(t_.hikerTimeoutMin * 60.0f)) {
        // Nobody answered the buzzer. Half the hit of a missed dodge (the owner
        // was away, not careless), but the hiker still got a blurry polaroid.
        add(Stat::MYST, -t_.hikerHit * 0.5f);
        rollEvidence(0, (int)Tier::PHOTOGRAPHIC);
        s_.hikerShownAt = 0;
        s_.hikerAt[i] = 0;
        push(EventKind::HIKER_MISSED);
      }
    }
  }
  maybeHatchOrEvolve(now);
}

void Pet::catchUp(uint32_t now) {
  s_.lampOnSince = 0;
  if (now <= s_.lastSeen || s_.hibernating) { if (now > s_.lastSeen) s_.lastSeen = now; return; }
  uint32_t elapsed = now - s_.lastSeen;
  uint32_t cap = (uint32_t)(t_.catchUpCapHours * 3600.0f);
  uint32_t sim = elapsed > cap ? cap : elapsed;
  float before[(int)Stat::COUNT];
  std::memcpy(before, s_.stat, sizeof(before));
  uint32_t t = s_.lastSeen, end = s_.lastSeen + sim;
  while (t < end) {
    uint32_t t2 = end - t > 3600 ? t + 3600 : end;
    rollover(t);
    step(t, t2, true);
    t = t2;
  }
  rollover(now);
  // The floor covers every loss while away, nightly penalties included.
  for (int i = 0; i < (int)Stat::COUNT; ++i)
    if (s_.stat[i] < t_.catchUpFloor && before[i] > s_.stat[i])
      s_.stat[i] = before[i] < t_.catchUpFloor ? before[i] : t_.catchUpFloor;
  if (elapsed > cap) {
    uint32_t skipped = elapsed - cap;   // ages advance by the cap at most
    s_.stageStart += skipped;
    s_.hatchAt += skipped;
    s_.hibernating = s_.form != (uint8_t)Form::EGG ? 1 : 0;
  }
  s_.lastSeen = now;
  // Nobody was there to see the hikers, including the hikers.
  s_.hikerShownAt = 0;
  for (int i = 0; i < 2; ++i) if (s_.hikerAt[i] && s_.hikerAt[i] <= now) s_.hikerAt[i] = 0;
}

void Pet::digOut(uint32_t now) {
  if (!s_.hibernating) return;
  s_.hibernating = 0;
  for (int i = 0; i < (int)Stat::COUNT; ++i) s_.stat[i] = t_.digOutStats;
  if ((Species)s_.species == Species::NESSIE) s_.loch = t_.digOutStats;
  s_.lastSeen = now;
}

// ---- lifecycle (B7) -------------------------------------------------------------
Form Pet::resolveFork(uint32_t now) const {
  float careAvg = s_.weight > 0 ? s_.careW / s_.weight : careNow();
  float treatRatio = s_.feeds ? (float)s_.treats / (float)s_.feeds : 0.0f;
  if (treatRatio >= t_.hoaxTreatRatio && s_.treats >= t_.hoaxMinTreats && careAvg >= t_.hoaxMinCare)
    return Form::HOAX;
  return forkScore(now) >= t_.legendScore ? Form::LEGEND : Form::BLURRY;
}
float Pet::forkScore(uint32_t now) const {
  float careAvg = s_.weight > 0 ? s_.careW / s_.weight : careNow();
  float mystAvg = s_.weight > 0 ? s_.mystW / s_.weight : s_.stat[(int)Stat::MYST];
  float neg = s_.neglectHours * 3.0f; if (neg > 100) neg = 100;
  float moon = nearFullMoon(now, t_.fullMoonWindowH) ? t_.moonBonus : 0.0f;
  return 0.5f * careAvg + 0.3f * mystAvg + 0.2f * (100.0f - neg) + moon;
}
Form Pet::forkPreview(uint32_t now) const { return resolveFork(now); }

void Pet::maybeHatchOrEvolve(uint32_t now) {
  Form f = (Form)s_.form;
  if (f == Form::EGG) {
    if (now >= s_.hatchAt && (s_.firstPet || awake(now))) {
      s_.form = (uint8_t)Form::WHISPER;
      s_.stageStart = now;
      s_.stageCare = s_.stageHours = 0;
      s_.stagePhotos |= 1u << (int)Form::WHISPER;
      push(EventKind::HATCHED, (uint8_t)Form::WHISPER);
    }
    return;
  }
  if (f >= Form::LEGEND) {
    float days = (Species)s_.species == Species::NESSIE ? t_.nessieRestlessDays : t_.restlessDays;
    float p = moonPhase(now);
    if (!s_.restless && s_.adultHours >= days * 24.0f && p >= 0.30f && p <= 0.70f &&
        awake(now) && isNightHour(localHour(now), t_)) {
      s_.restless = 1;
      push(EventKind::RESTLESS);
    }
    return;
  }
  float h = localHour(now);
  bool inWindow = t_.evoStart < t_.evoEnd ? (h >= t_.evoStart && h < t_.evoEnd)
                                         : (h >= t_.evoStart || h < t_.evoEnd);
  if (!inWindow || !awake(now)) return;
  float age = now > s_.stageStart ? (float)(now - s_.stageStart) / 3600.0f : 0.0f;
  float need = stageHoursNeeded(f);
  bool early = nearFullMoon(now, t_.fullMoonWindowH) && age >= need * t_.fullMoonEarly;
  if (age < need && !early) return;

  Form next;
  bool overdue = age >= need * t_.lateGrowMul;   // neglected pets still grow up, late
  if (f == Form::WHISPER) {
    if (s_.stat[(int)Stat::BELLY] <= t_.whisperExitBelly && !overdue) return;
    next = Form::RUMOR;
  } else if (f == Form::RUMOR) {
    float avg = s_.stageHours > 0 ? s_.stageCare / s_.stageHours : careNow();
    if (avg <= t_.rumorExitCare && !overdue) return;
    next = Form::SIGHTING;
  } else {
    next = resolveFork(now);
    s_.raisedMask |= 1u << s_.species;
    s_.lastOutcome[s_.species] = (uint8_t)next;
  }
  s_.form = (uint8_t)next;
  s_.stageStart = now;
  s_.stageCare = s_.stageHours = 0;
  s_.stagePhotos |= 1u << (int)next;
  push(EventKind::EVOLVED, (uint8_t)next);
}

Form Pet::release(uint32_t now) {
  (void)now;
  Form f = (Form)s_.form;
  if (f < Form::LEGEND) return Form::EGG;   // only adults can be released
  s_.restless = 0;
  return f;
}

// ---- sessions and care (B4, B5) ---------------------------------------------------
void Pet::expose(float v) {
  float room = t_.exposeCapPerDay - s_.exposureToday;
  if (room <= 0) return;
  if (v > room) v = room;
  s_.exposureToday += v;
  add(Stat::MYST, -v);
}

void Pet::beginSession(uint32_t now) {
  tick(now);
  if (s_.hibernating) return;
  if (s_.sessionsThisHour < 255) ++s_.sessionsThisHour;
  if (s_.form == (uint8_t)Form::EGG) return;
  bool night = isNightHour(localHour(now), t_);
  expose(night ? t_.exposeNight : t_.exposeDay);
  if (night && !s_.checkedIn) {
    s_.checkedIn = 1;
    float p = moonPhase(now);
    bool newMoon = p < 0.034f || p > 0.966f;               // about a day either side
    if (newMoon || rand01() < CHECKIN_CHANCE)
      rollEvidence(nearFullMoon(now, t_.fullMoonWindowH) ? 1 : 0);
  }
}

bool Pet::preCare(uint32_t now, Result& r, bool allowEgg) {
  tick(now);
  if (s_.hibernating) { r = Result::HIBERNATING; return false; }
  if (s_.form == (uint8_t)Form::EGG && !allowEgg) { r = Result::NOT_NOW; return false; }
  r = Result::OK;
  return true;
}

// Waking a sleeping pet costs Cheer and keeps it up for a while.
static Result wakeIfAsleep(Pet& p, State& s, const Tuning& t, uint32_t now) {
  if (p.awake(now)) return Result::OK;
  s.stat[(int)Stat::CHEER] = clamp100(s.stat[(int)Stat::CHEER] - t.wakePenaltyCheer);
  s.wakeUntil = now + (uint32_t)(t.wakeMinutes * 60.0f);
  return Result::WOKE_IT;
}

Result Pet::warmEgg(uint32_t now) {
  Result r;
  if (!preCare(now, r, true)) return r;
  if (s_.form != (uint8_t)Form::EGG) return Result::NOT_NOW;
  return Result::OK;   // cosmetic: wobble and an owl hoot; eggs do not decay
}

Result Pet::pat(uint32_t now) {
  Result r;
  if (!preCare(now, r)) return r;
  if (!awake(now)) return Result::OK;   // sleepy smile, no effect, does not wake it
  float gain = t_.patCapPerHour - s_.patThisHour;
  if (gain <= 0) return Result::COOLDOWN;
  if (gain > t_.patCheer) gain = t_.patCheer;
  s_.patThisHour += gain;
  add(Stat::CHEER, gain);
  return Result::OK;
}

Result Pet::cuddle(uint32_t now) {
  Result r;
  if (!preCare(now, r)) return r;
  float h = localHour(now);
  if (h >= t_.bedHour - 1.5f && h < t_.wakeHour) return lights(now);  // dawn tuck-in, or back to sleep by day
  if (now < s_.cuddleReadyAt) return Result::COOLDOWN;
  add(Stat::CHEER, t_.cuddleCheer);
  add(Stat::REST, t_.cuddleRest);
  s_.cuddleReadyAt = now + (uint32_t)(t_.cuddleCooldownMin * 60.0f);
  return Result::OK;
}

Result Pet::feed(uint32_t now, Food f) {
  Result r;
  if (!preCare(now, r)) return r;
  Species sp = (Species)s_.species;
  if (f == Food::TREAT && s_.treatsLeft == 0) return Result::COOLDOWN;
  if (f == Food::FORAGE && sp != Species::BIGFOOT) return Result::NOT_NOW;
  Result w = wakeIfAsleep(*this, s_, t_, now);
  if (f == Food::STAPLE) add(Stat::BELLY, t_.feedStaple);
  if (f == Food::FORAGE) {
    add(Stat::BELLY, t_.forageBelly);
    if (s_.actionRolls < t_.actionRollsPerNight && rand01() < 0.15f) { ++s_.actionRolls; rollEvidence(0); }
  }
  if (f == Food::TREAT) {
    add(Stat::BELLY, t_.feedTreat);
    add(Stat::CHEER, t_.treatCheer);
    --s_.treatsLeft;
    ++s_.treats;
  }
  ++s_.feeds;
  if (rand01() < t_.messChance) { add(Stat::TIDY, -t_.messTidy); push(EventKind::MESS); }
  return w;
}

Result Pet::play(uint32_t now, bool great) {
  Result r;
  if (!preCare(now, r)) return r;
  if (now < s_.dizzyUntil) return Result::NOT_NOW;     // Mothman sees spirals
  Result w = wakeIfAsleep(*this, s_, t_, now);
  if (s_.playsTonight < t_.playsPerNight) add(Stat::CHEER, great ? t_.playGreat : t_.playNormal);
  if (s_.playsTonight < 255) ++s_.playsTonight;
  add(Stat::REST, -t_.playRestCost);
  if (great && s_.actionRolls < t_.actionRollsPerNight) { ++s_.actionRolls; rollEvidence(1); }
  return w;
}

Result Pet::hide(uint32_t now) {
  Result r;
  if (!preCare(now, r)) return r;
  if (now < s_.hideReadyAt) return Result::COOLDOWN;
  Result w = wakeIfAsleep(*this, s_, t_, now);
  add(Stat::MYST, (Species)s_.species == Species::NESSIE ? t_.nessieHideMyst : t_.hideMyst);
  s_.hideReadyAt = now + (uint32_t)(t_.hideCooldownMin * 60.0f);
  if (s_.hikerShownAt) return resolveHiker(now, true);
  return w;
}

Result Pet::clean(uint32_t now) {
  Result r;
  if (!preCare(now, r)) return r;
  Result w = wakeIfAsleep(*this, s_, t_, now);
  add(Stat::TIDY, t_.cleanTidy);
  return w;
}

Result Pet::scrub(uint32_t now) {
  Result r;
  if (!preCare(now, r)) return r;
  if (now < s_.scrubReadyAt) return Result::COOLDOWN;
  Result w = wakeIfAsleep(*this, s_, t_, now);
  add(Stat::TIDY, t_.scrubTidy);
  s_.scrubReadyAt = now + (uint32_t)(t_.scrubCooldownMin * 60.0f);
  return w;
}

Result Pet::lights(uint32_t now) {
  Result r;
  if (!preCare(now, r)) return r;
  float h = localHour(now);
  bool tuckTime = h >= t_.bedHour - 1.5f && h < t_.bedHour + t_.untuckedGrace;
  bool day = h >= t_.bedHour - 1.5f && h < t_.wakeHour;
  if (tuckTime) {                      // the dawn tuck-in (B4): bonus only in this window
    s_.wakeUntil = 0;
    if (!s_.tucked) { s_.tucked = 1; add(Stat::REST, t_.tuckRestBonus); return Result::OK; }
    return Result::COOLDOWN;
  }
  if (day) {                           // later in the day: just back to sleep, no bonus
    s_.wakeUntil = 0;
    return Result::OK;
  }
  if (s_.napUsed) return Result::COOLDOWN;
  s_.napUsed = 1;
  add(Stat::REST, t_.napRest);
  return Result::OK;
}

// ---- species quirks (B6) -----------------------------------------------------------
Result Pet::setLamp(uint32_t now, bool on) {
  Result r;
  if (!preCare(now, r)) return r;
  if ((Species)s_.species != Species::MOTHMAN) return Result::NOT_NOW;
  if (!on) { s_.lampOnSince = 0; return Result::OK; }
  if (now < s_.dizzyUntil) return Result::NOT_NOW;
  if (!s_.lampOnSince) s_.lampOnSince = now;
  return Result::OK;
}

bool Pet::hikerPending(uint32_t now) const {
  return s_.hikerShownAt && now >= s_.hikerShownSince &&
         now - s_.hikerShownSince <= (uint32_t)(t_.hikerTimeoutMin * 60.0f);
}

Result Pet::resolveHiker(uint32_t now, bool hid) {
  if (!s_.hikerShownAt) return Result::NOT_NOW;
  for (int i = 0; i < 2; ++i) if (s_.hikerAt[i] == s_.hikerShownAt) s_.hikerAt[i] = 0;
  s_.hikerShownAt = 0;
  (void)now;
  if (hid) { add(Stat::MYST, t_.hikerDodge); return Result::OK; }
  add(Stat::MYST, -t_.hikerHit);
  rollEvidence(0, (int)Tier::PHOTOGRAPHIC);   // consolation: a blurry polaroid
  return Result::OK;
}

Result Pet::topUpLoch(uint32_t now) {
  Result r;
  if (!preCare(now, r)) return r;
  if ((Species)s_.species != Species::NESSIE) return Result::NOT_NOW;
  bool wasLow = s_.loch < t_.lochLow + 20.0f;      // pebbles only when it really needed it
  s_.loch = clamp100(s_.loch + t_.lochTopUp);
  if (wasLow && s_.actionRolls < t_.actionRollsPerNight && rand01() < LOCH_PEBBLE_CHANCE) {
    ++s_.actionRolls; rollEvidence(0);
  }
  return Result::OK;
}

Result Pet::walk(uint32_t now) {
  Result r;
  if (!preCare(now, r)) return r;
  if ((Species)s_.species != Species::NIGHTCRAWLER) return Result::NOT_NOW;
  if (s_.walked) return Result::COOLDOWN;
  Result w = wakeIfAsleep(*this, s_, t_, now);
  s_.walked = 1;
  if (s_.actionRolls < t_.actionRollsPerNight && rand01() < WALK_SOCK_CHANCE) { ++s_.actionRolls; rollEvidence(0); }
  return w;
}

Result Pet::wearPants(uint32_t now, uint8_t which) {
  Result r;
  if (!preCare(now, r)) return r;
  if ((Species)s_.species != Species::NIGHTCRAWLER || which > 3) return Result::NOT_NOW;
  if (s_.pantsDone) return Result::COOLDOWN;
  s_.pantsDone = 1;
  add(Stat::CHEER, which == s_.pantsWanted ? t_.pantsRight : -t_.pantsWrong);
  return Result::OK;
}

// ---- persistence --------------------------------------------------------------------
uint32_t Pet::crc32(const uint8_t* d, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; ++i) {
    c ^= d[i];
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

size_t Pet::serialize(uint8_t* out, size_t cap) const {
  if (cap < sizeof(State)) return 0;
  State tmp;
  std::memcpy(&tmp, &s_, sizeof(State));
  tmp.magic = CRYPTLINGS_MAGIC;
  tmp.version = CRYPTLINGS_VERSION;
  tmp.crc = crc32(reinterpret_cast<const uint8_t*>(&tmp), offsetof(State, crc));
  std::memcpy(out, &tmp, sizeof(State));
  return sizeof(State);
}

bool Pet::load(const uint8_t* in, size_t len) {
  if (len < sizeof(State)) return false;
  State tmp;
  std::memcpy(&tmp, in, sizeof(State));
  if (tmp.magic != CRYPTLINGS_MAGIC || tmp.version != CRYPTLINGS_VERSION) return false;
  if (tmp.crc != crc32(reinterpret_cast<const uint8_t*>(&tmp), offsetof(State, crc))) return false;
  if (tmp.species >= (uint8_t)Species::COUNT || tmp.form > (uint8_t)Form::HOAX) return false;
  s_ = tmp;
  qn_ = 0;
  return true;
}

}  // namespace cryptlings
