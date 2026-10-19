// CRYPTLINGS device app: every screen, overlay, minigame and sound cue.
// Portable C++: it draws into a gfx::Canvas and talks to the hardware only
// through Platform, so the same code runs on the board and in the simulator.
#pragma once
#include <cstdint>

#include "cryptlings_core.h"
#include "gfx.h"
#include "platform.h"

struct TouchIn { bool down; int x, y; };

class App {
 public:
  App(Platform& p, gfx::Canvas& c) : P(p), cv(c) {}

  // Call once after loading. petData may be null (first boot).
  void begin(uint32_t ms, const uint8_t* petData, size_t petLen, const Settings* s);
  // Call often (every 10 to 30 ms). Returns true when the frame changed and
  // should be pushed to the display.
  bool update(uint32_t ms, const TouchIn& t);
  bool screenOn() const { return screenOn_; }
  void render();                     // draws the current frame into the canvas

  // For the serial console and tests.
  cryptlings::Pet& pet() { return pet_; }
  void forceSave();
  void showToast(const char* msg, uint32_t ms = 2600);

 private:
  enum class Scr : uint8_t { BOOT, TRAY, NAME, MAIN, GAME };
  enum class Tab : uint8_t { DEN, CARE, FILE, MORE };
  enum class Ov : uint8_t { NONE, REVEAL, HIKER, BURROW, FAREWELL, PANTS, CLOCK, RESULT, CONFIRM_RESET };
  enum class Game : uint8_t { WALTZ, FORAGE, HOP, STROLL };

  // ---- core ----
  Platform& P;
  gfx::Canvas& cv;
  cryptlings::Pet pet_;
  Settings set_{};
  Scr scr_ = Scr::BOOT;
  Tab tab_ = Tab::DEN;
  Ov ov_ = Ov::NONE;
  uint32_t ms_ = 0, now_ = 0;
  bool dirty_ = true;
  bool screenOn_ = true;
  uint32_t lastInput_ = 0, lastTick_ = 0, lastSave_ = 0, lastAnim_ = 0, lastSessionMs_ = 0;
  bool sessionOpen_ = false;
  bool petValid_ = false;          // pet_ holds a real (possibly released) pet: save it
  uint32_t ovSince_ = 0;           // when the current overlay appeared
  uint8_t pendingReveal_ = 0xFF;   // a hatch/evolution waiting for the screen to be free

  // ---- pointer ----
  struct Ptr {
    bool down = false, moved = false, swallow = false;
    int sx = 0, sy = 0, x = 0, y = 0, lx = 0, ly = 0;
    uint32_t t0 = 0;
    int reversals = 0, lastDir = 0;
  } ptr_;
  int holdId_ = 0;          // which control is being held (0 none)
  uint32_t holdNeed_ = 0;   // ms
  float holdP_ = 0;         // 0..1 progress for drawing

  // ---- ui state ----
  char toast_[120] = {0};
  uint32_t toastUntil_ = 0;
  int scroll_[4] = {0, 0, 0, 0};
  int trayPick_ = 0, trayScroll_ = 0, nameIdx_ = 0;
  uint32_t bootStart_ = 0;
  uint8_t revealForm_ = 0;
  uint32_t revealStart_ = 0;
  uint32_t hikerShownMs_ = 0;
  bool hikerWaiting_ = false;
  uint32_t lastHootMs_ = 0;
  uint8_t soundsToday_ = 0;
  int32_t soundDay_ = -1;
  bool lampOn_ = false;
  char farewell_[160] = {0};
  int contentH_[4] = {0, 0, 0, 0};

  // ---- minigame ----
  struct GameState {
    Game type = Game::WALTZ;
    uint32_t start = 0, lastSpawn = 0, lastStep = 0;
    int score = 0, misses = 0, steps = 0, wobbles = 0;
    int lastSide = -1;
    float px = 86, py = 160, inRingMs = 0, totalMs = 0, dizzy = 0;
    struct Obj { int16_t x, y; uint8_t kind, alive; uint32_t born; } obj[8];
    bool done = false, great = false, walk = false, wasDown = false;
    uint32_t lastMs = 0, rng = 1;
  } g_;

  // ---- flow ----
  void tickEngine(bool force = false);
  void handleEvents();
  void onTap(int x, int y);
  void onSwipe(int dir);       // -1 left, +1 right
  void onHoldDone(int id);
  void onDrag(int dx, int dy);
  void wake();
  void sleepScreen();
  void ensureSession();
  void save(bool force = false);
  void act(int action);
  void resultToast(cryptlings::Result r, const char* okMsg);
  void startGame();
  void updateGame();
  void finishGame();
  void newPet(int species);
  bool asleepNow() const;
  int  tod() const;           // 0 dusk 1 night 2 dawn 3 day
  int  formIdx() const;
  int  moodIdx() const;
  const char* hintLine(char* buf, int n);
  int  bestAction() const;
  void playMotif(int id, bool chime);

  // ---- drawing ----
  void drawStatus();
  void drawTabs();
  void drawBoot();
  void drawTray();
  void drawName();
  void drawDen();
  void drawCare();
  void drawFile();
  void drawMore();
  void drawGame();
  void drawOverlay();
  void drawToast();
  void drawPhoto(int x, int y, bool live);
  void drawCreature(int x, int y, int form, int mood, uint8_t alpha, int div = 1);
  void drawButton(int x, int y, int w, int h, const char* label, bool primary, const void* icon = nullptr);
  void drawHoldButton(int x, int y, int w, int h, const char* label, int id);
  void drawRing(int cx, int cy, int r, float p, gfx::Color c);
  void drawPaperCard(int x, int y, int w, int h);
  int  careRows(int* ids, int max) const;
};

// Action ids (shared by the DEN button, the CARE list and the console).
enum Act { A_NONE = 0, A_FEED, A_TREAT, A_FORAGE, A_PLAY, A_HIDE, A_CLEAN, A_LIGHTS, A_LAMP, A_LOCH,
           A_WALK, A_PANTS, A_WARM, A_CUDDLE, A_PAT, A_SCRUB };
