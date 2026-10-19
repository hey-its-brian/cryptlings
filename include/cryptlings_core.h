// CRYPTLINGS core engine (P1)
// Pure logic, no Arduino or hardware dependencies, so it builds and runs on the
// host for testing and drops unchanged into the firmware. Modeled on the
// NETSPAWN core (netspawn_core.h): the platform layer (firmware main.cpp or the
// host test) owns the clock, storage, touch and buzzer, and feeds this engine
// times and events. All tuning lives in Tuning and is marked TUNABLE.
//
// Design source: Obsidian vault, 01 Projects/CRYPTLINGS/CRYPTLINGS_Brief_v1.md
// (Part B: UX, Mechanics and Economy). Section numbers below (B4, B7...) refer
// to that brief.
//
// Firm rules carried in this code:
//  - Direct care is always enough. Nothing here needs radio or network.
//  - No death. Neglect ends in the BLURRY PHOTO adult, never a dead pet.
//  - Evolutions only happen live, inside the evening window, never in catch-up.

#ifndef CRYPTLINGS_CORE_H
#define CRYPTLINGS_CORE_H

#include <cstdint>
#include <cstddef>

namespace cryptlings {

// ---- identities -----------------------------------------------------------
enum class Species : uint8_t { MOTHMAN = 0, BIGFOOT, NESSIE, NIGHTCRAWLER, COUNT };

enum class Form : uint8_t {
  EGG = 0,
  WHISPER,      // hatchling
  RUMOR,        // kid
  SIGHTING,     // teen
  LEGEND,       // well raised adult
  BLURRY,       // neglected adult ("BLURRY PHOTO")
  HOAX          // spoiled adult (stretch ending)
};

enum class Stat : uint8_t { BELLY = 0, CHEER, REST, MYST, TIDY, COUNT };

enum class Food : uint8_t { STAPLE = 0, TREAT, FORAGE /* Bigfoot only */ };

enum class Tier : uint8_t { HEARSAY = 0, ANECDOTE, EYEWITNESS, PHOTOGRAPHIC, IRREFUTABLE };

// Outcome of a care action, so the UI can pick the right reaction and copy.
enum class Result : uint8_t {
  OK = 0,
  WOKE_IT,       // done, but the pet was asleep: Cheer penalty, now awake 20 min
  COOLDOWN,      // too soon, nothing happened
  NOT_NOW,       // not valid in this state (egg, dizzy Mothman, wrong species)
  HIBERNATING    // dig it out first (digOut)
};

// ---- tuning (all TUNABLE; rates are per real hour unless noted) ------------
struct Tuning {
  // schedule (local clock, hours 0..24) B4
  float wakeHour        = 18.0f;  // dusk: wakes up
  float bedHour         = 7.0f;   // dawn: bedtime
  float untuckedGrace   = 1.0f;   // falls asleep on its own this long after bedHour
  float untuckedRestMul = 0.7f;   // "asleep on a branch" rest rate
  float quietStart      = 23.0f;  // owner quiet hours: halved awake decay, no sounds
  float quietEnd        = 7.0f;
  float evoStart        = 20.0f;  // evolution window B7
  float evoEnd          = 23.0f;

  // decay awake / asleep B4
  float bellyAwake = 6.0f, bellyAsleep = 2.0f;
  float cheerAwake = 3.0f, cheerAsleep = 1.0f;
  float restAwake  = 5.0f, restAsleepGain = 12.0f;
  float mystBase   = 1.0f;       // hiding matters: about one HIDE a night holds it
  float tidyAwake  = 2.5f, tidyAsleep = 1.0f;
  float quietNightMyst = 0.75f;   // +/h for a night hour with <= 1 session

  // care actions B5
  float feedStaple = 40.0f, feedTreat = 15.0f, treatCheer = 10.0f;  // staple: two a night should get by
  float forageBelly = 25.0f;      // Bigfoot FORAGE
  uint8_t treatsPerNight = 3;
  float messChance = 0.40f, messTidy = 10.0f;
  float patCheer = 3.0f, patCapPerHour = 15.0f;
  float cuddleCheer = 8.0f, cuddleRest = 2.0f, cuddleCooldownMin = 30.0f;
  float playNormal = 25.0f, playGreat = 35.0f, playRestCost = 6.0f;
  uint8_t playsPerNight = 4;      // plays that count for Cheer
  float hideMyst = 15.0f, hideCooldownMin = 120.0f;
  float cleanTidy = 40.0f;
  float scrubTidy = 10.0f, scrubCooldownMin = 15.0f;
  float tuckRestBonus = 10.0f;
  float napRest = 6.0f;           // one short night nap
  float wakePenaltyCheer = 8.0f, wakeMinutes = 20.0f;

  // exposure B4
  float exposeDay = 2.0f, exposeNight = 0.5f, exposeCapPerDay = 12.0f;

  // species quirks B6
  float bigfootBellyMul = 1.2f;
  float hikerHit = 15.0f, hikerDodge = 5.0f, hikerTimeoutMin = 10.0f;
  float lochDrain = 2.0f, lochTopUp = 50.0f, lochLow = 30.0f, lochLowMul = 1.5f;
  float nessieHideMyst = 20.0f;
  float lampCheer = 4.0f, lampDizzyAfterMin = 3.0f, dizzyMinutes = 15.0f, dizzyCheer = 5.0f;
  float missedWalkCheer = 15.0f;
  float pantsRight = 10.0f, pantsWrong = 3.0f;

  // lifecycle B7 (hours)
  float eggFirstMin = 10.0f;      // first pet hatches fast, any time of day
  float eggHours    = 6.0f;
  float whisperHours = 36.0f, rumorHours = 72.0f, sightingHours = 72.0f;
  float nessieGrowth = 0.8f;      // slower: stage durations / growth
  float restlessDays = 5.0f, nessieRestlessDays = 8.0f;
  float whisperExitBelly = 30.0f;
  float rumorExitCare = 25.0f;
  uint8_t actionRollsPerNight = 3; // great play, forage, loch, walk rolls
  float lateGrowMul = 2.0f;       // past this x stage time it grows up anyway (no stuck pets)
  float fullMoonWindowH = 18.0f, fullMoonEarly = 0.7f, moonBonus = 5.0f;

  // fork B7
  float legendScore = 62.0f;
  float hoaxTreatRatio = 0.40f, hoaxMinTreats = 15.0f, hoaxMinCare = 50.0f;
  float neglectStat = 15.0f;

  // catch-up B9
  float catchUpCapHours = 72.0f, catchUpDecayMul = 0.5f, catchUpFloor = 10.0f;
  float digOutStats = 50.0f;
};

// ---- persisted state (POD, safe to memcpy to NVS / SD / a host file) -------
static const int EVIDENCE_ITEMS = 16;   // per species, see evidenceTier()

struct State {
  uint32_t magic;
  uint16_t version;
  uint8_t  species;          // Species
  uint8_t  form;             // Form

  float stat[(int)Stat::COUNT];   // 0..100
  float loch;                     // Nessie only, 0..100

  // clocks (unix seconds, UTC) and local offset
  int32_t  utcOffsetSec;     // platform sets from timezone; local = now + offset
  uint32_t lastSeen;         // last reconcile
  uint32_t hatchAt;          // egg ready time
  uint32_t stageStart;       // when the current form began
  uint32_t wakeUntil;        // woken in the day: awake until this time
  uint32_t cuddleReadyAt, hideReadyAt, scrubReadyAt;
  uint32_t lampOnSince;      // 0 = lamp off
  uint32_t dizzyUntil;
  uint32_t hikerAt[2];       // tonight's Bigfoot hiker events (0 = none)
  uint32_t hikerShownAt;     // scheduled time of the alert now showing (0 = none)
  uint32_t hikerShownSince;  // when that alert actually appeared (timeout counts from here)
  uint32_t rng;

  // per-period counters, keyed by an id so they reset themselves
  int32_t  nightId;          // night counters belong to this night
  int32_t  hourId;           // hour counters belong to this hour
  int32_t  dayId;            // daily exposure belongs to this local date
  float    patThisHour;
  uint8_t  sessionsThisHour;
  uint8_t  playsTonight;
  uint8_t  treatsLeft;
  uint8_t  napUsed;
  float    exposureToday;
  uint8_t  tucked;           // tucked in for this dawn
  uint8_t  walked;           // Nightcrawler walked tonight
  uint8_t  pantsWanted;      // 0..3, Nightcrawler
  uint8_t  pantsDone;
  uint8_t  checkedIn;        // first session tonight already rolled evidence
  uint8_t  firstPet;
  uint8_t  hibernating;
  uint8_t  restless;
  uint8_t  actionRolls;      // evidence rolls from actions tonight (capped, no grinding)

  // fork aggregates B7
  float careW, mystW, weight;     // weighted integrals and total weight
  float stageCare, stageHours;    // plain average over the current stage
  float neglectHours;
  float adultHours;
  uint16_t feeds, treats;
  uint8_t  careStreak;
  int32_t  lastStreakNight;
  float    nightCareSum, nightCareHours;

  // collection (kept across pets)
  uint8_t evidence[(int)Species::COUNT][EVIDENCE_ITEMS];  // corroboration counts
  uint8_t stagePhotos;            // bitmask of Forms photographed for this pet
  uint8_t raisedMask;             // species raised before ("PREVIOUSLY OBSERVED")
  uint8_t lastOutcome[(int)Species::COUNT];  // last adult Form per species, 0 = none

  uint32_t crc;
};

static const uint32_t CRYPTLINGS_MAGIC   = 0x43525950; // "CRYP"
static const uint16_t CRYPTLINGS_VERSION = 1;

// An event the UI should show (toast, sound, overlay). Polled with nextEvent().
enum class EventKind : uint8_t {
  NONE = 0, HATCHED, EVOLVED, EVIDENCE, HIKER, HIKER_MISSED, DIZZY, MISSED_WALK,
  WENT_TO_SLEEP, WOKE_AT_DUSK, RESTLESS, MESS
};
struct Event { EventKind kind; uint8_t a; uint8_t b; };  // a/b: form, item, tier...

// ---- engine ---------------------------------------------------------------
class Pet {
public:
  explicit Pet(const Tuning& t = Tuning());

  // New egg. Keeps collection (evidence, raisedMask, lastOutcome).
  void spawnEgg(Species sp, uint32_t now, int32_t utcOffsetSec, bool firstPet);

  // Live advance to `now` (call every few seconds to a minute). Runs
  // hatching, evolution, quirk events. Steps in <= 15 min chunks.
  void tick(uint32_t now);

  // Boot reconcile after power-off. Capped catch-up at 50% decay; over the cap
  // the pet is hibernating until digOut(). No evolution happens here.
  void catchUp(uint32_t now);
  void digOut(uint32_t now);

  // A screen wake that includes an interaction (exposure, check-in roll).
  void beginSession(uint32_t now);

  // Care actions B5.
  Result warmEgg(uint32_t now);                 // hold on the egg (cosmetic)
  Result pat(uint32_t now);
  Result cuddle(uint32_t now);                  // hold at night; at dawn it tucks in
  Result feed(uint32_t now, Food f);
  Result play(uint32_t now, bool great);        // after the minigame
  Result hide(uint32_t now);
  Result clean(uint32_t now);
  Result scrub(uint32_t now);                   // drag gesture
  Result lights(uint32_t now);                  // tuck in at dawn, nap at night

  // Species quirks B6.
  Result setLamp(uint32_t now, bool on);        // Mothman
  Result resolveHiker(uint32_t now, bool hid);  // Bigfoot, after the alert
  Result topUpLoch(uint32_t now);               // Nessie
  Result walk(uint32_t now);                    // Nightcrawler (stroll or 300 steps)
  Result wearPants(uint32_t now, uint8_t which);// Nightcrawler, 0..3

  // End of life (release into the fog). Returns the outcome Form.
  Form release(uint32_t now);

  // Queries.
  bool  awake(uint32_t now) const;
  bool  inQuiet(uint32_t now) const;
  bool  hikerPending(uint32_t now) const;       // show the 10 s alert now
  float careNow() const;                        // mean of Belly, Cheer, Rest, Tidy
  float forkScore(uint32_t now) const;          // what the fork would say today
  Form  forkPreview(uint32_t now) const;
  bool  nextEvent(Event& out);

  const State& state() const { return s_; }
  State&       state()       { return s_; }
  const Tuning& tuning() const { return t_; }

  size_t serialize(uint8_t* out, size_t cap) const;
  bool   load(const uint8_t* in, size_t len);

  // Helpers, public for tests and UI.
  float localHour(uint32_t now) const;
  int32_t nightOf(uint32_t now) const;           // a night runs noon to noon
  static float moonPhase(uint32_t now);          // 0 new, 0.5 full
  static bool  nearFullMoon(uint32_t now, float windowHours);
  static Tier  evidenceTier(int item);

private:
  void   step(uint32_t t0, uint32_t t1, bool catchingUp);
  void   rollover(uint32_t now);                 // night / hour / day counters
  void   maybeHatchOrEvolve(uint32_t now);
  Form   resolveFork(uint32_t now) const;
  float  stageHoursNeeded(Form f) const;
  float  growth() const;
  bool   preCare(uint32_t now, Result& r, bool allowEgg = false);
  void   add(Stat s, float v);
  void   expose(float v);
  uint32_t rand32();
  float  rand01();
  int    rollEvidence(int tierShift, int minTier = 0);
  void   push(EventKind k, uint8_t a = 0, uint8_t b = 0);
  static uint32_t crc32(const uint8_t* data, size_t len);

  State  s_;
  Tuning t_;
  Event  q_[8];
  uint8_t qn_ = 0;
};

const char* formName(Form f);
const char* speciesName(Species s);

} // namespace cryptlings

#endif // CRYPTLINGS_CORE_H
