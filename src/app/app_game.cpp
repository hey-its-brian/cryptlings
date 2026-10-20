// The four 20-second minigames (Brief B5), one per species, single touch.
// Rules only; each screen layout draws them (app_draw.cpp, app_draw_round.cpp).
#include <cmath>
#include <cstdio>
#include <cstring>

#include "app.h"
#include "data.h"
#include "layout.h"

using namespace cryptlings;

namespace {
uint32_t rnd(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
}  // namespace

void App::startGame() {
  bool walk = g_.walk;
  Game type = g_.type;
  g_ = GameState();
  g_.walk = walk;
  g_.type = type;
  g_.start = ms_;
  g_.lastMs = ms_;
  g_.rng = (uint32_t)(now_ ^ ms_) | 1u;
  g_.px = L::G_PX0;
  g_.py = L::G_PY0;
  scr_ = Scr::GAME;
  ensureSession();
}

void App::updateGame() {
  uint32_t t = ms_ - g_.start;
  float dt = (float)(ms_ - g_.lastMs);
  g_.lastMs = ms_;
  bool pressed = ptr_.down && !g_.wasDown;   // a new touch this frame
  g_.wasDown = ptr_.down;
  if (pressed && hitAt(ptr_.sx, ptr_.sy).id == HIT_QUIT) { g_.done = true; finishGame(); return; }
  float ts = t / 1000.0f;

  switch (g_.type) {
    case Game::WALTZ: {
      float gx = L::G_WX + L::G_WAX * std::sin(ts * 0.9f), gy = L::G_WY + L::G_WAY * std::sin(ts * 0.6f + 1.0f);
      if (ptr_.down) { g_.px = (float)ptr_.x; g_.py = (float)ptr_.y; }
      float d = std::sqrt((g_.px - gx) * (g_.px - gx) + (g_.py - gy) * (g_.py - gy));
      if (d < L::G_RING && d > L::G_CORE) g_.inRingMs += dt;
      if (d <= L::G_CORE) g_.dizzy += dt / 1800.0f;
      g_.totalMs += dt;
      g_.score = (int)(g_.inRingMs * 100 / (GAME_MS));
      if (g_.dizzy >= 1.0f) { finishGame(); return; }
      break;
    }
    case Game::FORAGE: {
      for (auto& o : g_.obj) if (o.alive && ms_ - o.born > 950) o.alive = 0;
      if (ms_ - g_.lastSpawn > 520 + rnd(g_.rng) % 300) {
        int slot = (int)(rnd(g_.rng) % 6);
        auto& o = g_.obj[slot];
        if (!o.alive) {
          o.alive = 1;
          o.born = ms_;
          uint32_t r = rnd(g_.rng) % 100;
          o.kind = r < 25 ? 2 : (r < 60 ? 1 : 0);   // 0 berry, 1 mushroom, 2 pinecone
          o.x = (int16_t)(slot % 2 == 0 ? L::G_FX0 : L::G_FX1);
          o.y = (int16_t)(L::G_FY0 + (slot / 2) * L::G_FDY);
        }
        g_.lastSpawn = ms_;
      }
      if (pressed) {
        for (auto& o : g_.obj) {
          if (!o.alive) continue;
          int dx = ptr_.sx - o.x, dy = ptr_.sy - o.y;
          if (dx * dx + dy * dy < L::G_FHIT * L::G_FHIT) {
            o.alive = 0;
            if (o.kind == 2) ++g_.misses; else ++g_.score;
          }
        }
      }
      break;
    }
    case Game::HOP: {
      for (auto& o : g_.obj) {
        if (!o.alive) continue;
        o.x -= (int16_t)(dt * (o.kind == 1 ? L::G_HSPD_BOAT : L::G_HSPD_BUB) + 0.5f);
        if (o.x < L::G_HGONE) o.alive = 0;
      }
      if (ms_ - g_.lastSpawn > 700) {
        for (auto& o : g_.obj)
          if (!o.alive) {
            o.alive = 1;
            o.kind = (rnd(g_.rng) % 100) < 22 ? 1 : 0;   // 1 rowboat, 0 bubble
            o.x = L::G_HSPAWN;
            o.y = o.kind ? L::G_HBOAT_Y : (int16_t)(L::G_HBUB_Y + rnd(g_.rng) % L::G_HBUB_SPAN);
            o.born = ms_;
            break;
          }
        g_.lastSpawn = ms_;
      }
      if (pressed) {
        for (auto& o : g_.obj) {
          if (!o.alive) continue;
          int dx = ptr_.sx - o.x, dy = ptr_.sy - o.y;
          int r = o.kind ? L::G_HHIT_BOAT : L::G_HHIT_BUB;
          if (dx * dx + dy * dy < r * r) {
            o.alive = 0;
            if (o.kind) ++g_.misses; else ++g_.score;
            break;
          }
        }
      }
      break;
    }
    case Game::STROLL: {
      if (pressed && ptr_.sy > L::G_TOP && ptr_.sy < L::G_BOT) {
        int side = ptr_.sx < L::G_SPLIT ? 0 : 1;
        uint32_t gap = ms_ - g_.lastStep;
        if (side == g_.lastSide || (g_.lastStep && gap < 250)) ++g_.wobbles;
        else { ++g_.steps; ++g_.score; }
        g_.lastSide = side;
        g_.lastStep = ms_;
      }
      break;
    }
  }
  if (t >= GAME_MS) finishGame();
}

void App::finishGame() {
  if (scr_ != Scr::GAME) return;
  scr_ = Scr::MAIN;
  if (ptr_.down) ptr_.swallow = true;   // the finger that ended the game must not dismiss the card
  ovSince_ = ms_;
  tab_ = Tab::DEN;
  bool quit = g_.done;
  const SpeciesText& T = SPECIES_TEXT[pet_.state().species & 3];
  if (g_.walk) {
    if (!quit && g_.steps >= 10) {
      Result r = pet_.walk(now_);
      std::snprintf(farewell_, sizeof(farewell_), "%d calm steps around the block. %s", g_.steps,
                    r == Result::WOKE_IT ? "He was asleep, but he does not mind a stroll." : "He nods, once. High praise.");
    } else {
      std::snprintf(farewell_, sizeof(farewell_), "Only %d steps. He wandered back in. Try again later.", g_.steps);
    }
    ov_ = Ov::RESULT;
    handleEvents();
    save(true);
    return;
  }
  bool great = false;
  const char* verdict = "";
  switch (g_.type) {
    case Game::WALTZ:
      great = g_.inRingMs >= GAME_MS * 0.7f;
      verdict = g_.dizzy >= 1.0f ? "Too close to the bulb. He is a little dizzy." : great ? "A perfect waltz around the light." : "A nice twirl in the glow.";
      break;
    case Game::FORAGE:
      great = g_.score >= 12 && g_.misses <= 2;
      verdict = g_.misses > 2 ? "Too many pinecones. Not snacks." : great ? "A bumper harvest." : "A decent pawful.";
      break;
    case Game::HOP:
      great = g_.score >= 10 && g_.misses == 0;
      verdict = g_.misses ? "Bumped a rowboat. Very apologetic." : great ? "Every bubble, no boats. Graceful." : "Some bubbles gulped.";
      break;
    case Game::STROLL:
      great = g_.steps >= 30 && g_.wobbles <= 3;
      verdict = g_.wobbles > 3 ? "A bit wobbly. Slow and steady, please." : great ? "Thirty steady steps. Dignified." : "A pleasant little stroll.";
      break;
  }
  if (quit) {
    std::snprintf(farewell_, sizeof(farewell_), "%s stopped early. No harm done.", T.game);
    ov_ = Ov::RESULT;
    return;
  }
  Result r = pet_.play(now_, great);
  int score = g_.type == Game::WALTZ ? g_.score : g_.score;
  std::snprintf(farewell_, sizeof(farewell_), "%s: %d%s. %s%s%s", T.game, score,
                g_.type == Game::WALTZ ? "% in the glow" : "", verdict, great ? " GREAT!" : "",
                r == Result::WOKE_IT ? " (It was asleep. Grumpy, but it played.)" : "");
  ov_ = Ov::RESULT;
  handleEvents();
  save(true);
}
