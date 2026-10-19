// The four 20-second minigames (Brief B5), one per species, single touch.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "app.h"
#include "art.h"
#include "data.h"
#include "theme.h"

using namespace cryptlings;

namespace {
const uint32_t GAME_MS = 20000;
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
  g_.px = 86;
  g_.py = 200;
  scr_ = Scr::GAME;
  ensureSession();
}

void App::updateGame() {
  uint32_t t = ms_ - g_.start;
  float dt = (float)(ms_ - g_.lastMs);
  g_.lastMs = ms_;
  bool pressed = ptr_.down && !g_.wasDown;   // a new touch this frame
  g_.wasDown = ptr_.down;
  if (pressed && ptr_.sy < 40 && ptr_.sx > 120) { g_.done = true; finishGame(); return; }   // quit
  float ts = t / 1000.0f;

  switch (g_.type) {
    case Game::WALTZ: {
      float gx = 86 + 46 * std::sin(ts * 0.9f), gy = 180 + 70 * std::sin(ts * 0.6f + 1.0f);
      if (ptr_.down) { g_.px = (float)ptr_.x; g_.py = (float)ptr_.y; }
      float d = std::sqrt((g_.px - gx) * (g_.px - gx) + (g_.py - gy) * (g_.py - gy));
      if (d < 36 && d > 12) g_.inRingMs += dt;
      if (d <= 12) g_.dizzy += dt / 1800.0f;
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
          o.x = (int16_t)(slot % 2 == 0 ? 46 : 126);
          o.y = (int16_t)(96 + (slot / 2) * 72);
        }
        g_.lastSpawn = ms_;
      }
      if (pressed) {
        for (auto& o : g_.obj) {
          if (!o.alive) continue;
          int dx = ptr_.sx - o.x, dy = ptr_.sy - o.y;
          if (dx * dx + dy * dy < 30 * 30) {
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
        o.x -= (int16_t)(dt * (o.kind == 1 ? 0.045f : 0.06f) + 0.5f);
        if (o.x < -30) o.alive = 0;
      }
      if (ms_ - g_.lastSpawn > 700) {
        for (auto& o : g_.obj)
          if (!o.alive) {
            o.alive = 1;
            o.kind = (rnd(g_.rng) % 100) < 22 ? 1 : 0;   // 1 rowboat, 0 bubble
            o.x = 190;
            o.y = o.kind ? 178 : (int16_t)(200 + rnd(g_.rng) % 90);
            o.born = ms_;
            break;
          }
        g_.lastSpawn = ms_;
      }
      if (pressed) {
        for (auto& o : g_.obj) {
          if (!o.alive) continue;
          int dx = ptr_.sx - o.x, dy = ptr_.sy - o.y;
          int r = o.kind ? 26 : 22;
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
      if (pressed && ptr_.sy > 40) {
        int side = ptr_.sx < 86 ? 0 : 1;
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

void App::drawGame() {
  const State& s = pet_.state();
  const SpeciesText& T = SPECIES_TEXT[s.species & 3];
  uint32_t t = ms_ - g_.start;
  float ts = t / 1000.0f;
  int form = formIdx();
  if (form == 0) form = 1;
  cv.fill(th::bg);
  // header
  cv.fillRect(0, 0, gfx::W, 40, th::panel);
  cv.text(F16H, 6, 18, T.game, th::moon);
  char sc[32];
  if (g_.type == Game::WALTZ) std::snprintf(sc, sizeof(sc), "Glow %d%%", g_.score);
  else if (g_.type == Game::STROLL) std::snprintf(sc, sizeof(sc), "Steps %d  Wobble %d", g_.steps, g_.wobbles);
  else std::snprintf(sc, sizeof(sc), "Got %d  Oops %d", g_.score, g_.misses);
  cv.text(F12B, 6, 34, sc, th::lantern);
  cv.text(F12B, 136, 16, "QUIT", th::dim);
  int left = (int)(GAME_MS - (t > GAME_MS ? GAME_MS : t));
  cv.fillRect(0, 38, gfx::W * left / (int)GAME_MS, 2, th::lantern);

  switch (g_.type) {
    case Game::WALTZ: {
      cv.vgradient(0, 40, gfx::W, 280, gfx::hex(0x070A14), gfx::hex(0x18213A), 6);
      int gx = (int)(86 + 46 * std::sin(ts * 0.9f)), gy = (int)(180 + 70 * std::sin(ts * 0.6f + 1.0f));
      cv.fillCircle(gx, gy, 44, th::lantern, 30);
      cv.fillCircle(gx, gy, 36, th::lantern, 40);
      cv.circle(gx, gy, 36, th::lantern);
      cv.fillCircle(gx, gy, 12, gfx::hex(0xF5D08A));
      cv.circle(gx, gy, 12, th::ember);
      cv.sprite(*creatureSprite(s.species, form, g_.dizzy > 0.5f ? 2 : 0), (int)g_.px - 50, (int)g_.py - 60);
      cv.fillRect(6, 308, 160, 6, th::panel);
      cv.fillRect(6, 308, (int)(160 * (g_.dizzy > 1 ? 1 : g_.dizzy)), 6, th::ember);
      cv.text(F12B, 6, 304, "Stay in the glow.", th::fog);
      break;
    }
    case Game::FORAGE: {
      cv.vgradient(0, 40, gfx::W, 280, gfx::hex(0x0D1424), gfx::hex(0x1C2A20), 5);
      for (int i = 0; i < 6; ++i) {
        int x = i % 2 == 0 ? 46 : 126, y = 96 + (i / 2) * 72;
        cv.fillEllipse(x, y + 14, 36, 22, gfx::hex(0x1C3A24));
        cv.fillEllipse(x - 10, y + 6, 18, 14, gfx::hex(0x245030));
        cv.fillEllipse(x + 12, y + 8, 18, 14, gfx::hex(0x2A5A36));
      }
      for (auto& o : g_.obj) {
        if (!o.alive) continue;
        if (o.kind == 0) { cv.fillCircle(o.x - 5, o.y, 7, gfx::hex(0x6B5BA8)); cv.fillCircle(o.x + 5, o.y + 2, 7, gfx::hex(0x5A4A98)); }
        else if (o.kind == 1) { cv.fillEllipse(o.x, o.y - 3, 12, 8, th::ember); cv.fillRect(o.x - 3, o.y, 6, 10, th::paper); cv.fillCircle(o.x - 4, o.y - 5, 2, th::paper); }
        else { cv.fillEllipse(o.x, o.y, 8, 12, gfx::hex(0x7A5A3A)); for (int k = -8; k < 10; k += 5) cv.hline(o.x - 6, o.y + k, 12, gfx::hex(0x4A3422)); }
      }
      cv.text(F12B, 6, 314, "Snacks! No pinecones.", th::fog);
      break;
    }
    case Game::HOP: {
      cv.vgradient(0, 40, gfx::W, 150, gfx::hex(0x070A14), gfx::hex(0x18213A), 5);
      cv.fillRect(0, 190, gfx::W, 130, gfx::hex(0x1B3A40));
      for (int x = 0; x < gfx::W; x += 12) cv.hline(x + (int)(ts * 20) % 12, 192, 6, th::loch);
      cv.sprite(*creatureSprite(s.species, form, 0), -10, 120, gfx::Tint(), 2);
      for (auto& o : g_.obj) {
        if (!o.alive) continue;
        if (o.kind) {
          cv.fillRect(o.x - 20, o.y + 2, 40, 8, gfx::hex(0x6B4A2A));
          cv.fillRect(o.x - 14, o.y - 6, 28, 8, gfx::hex(0x8A6A4A));
          cv.vline(o.x, o.y - 22, 16, th::paper);
        } else {
          cv.circle(o.x, o.y, 9, th::moon);
          cv.fillCircle(o.x - 3, o.y - 3, 2, th::moon);
        }
      }
      cv.text(F12B, 6, 314, "Bubbles. Not boats.", th::fog);
      break;
    }
    case Game::STROLL: {
      cv.vgradient(0, 40, gfx::W, 200, gfx::hex(0x070A14), gfx::hex(0x18213A), 5);
      cv.fillRect(0, 240, gfx::W, 80, gfx::hex(0x1D211F));
      for (int i = 0; i < 6; ++i) {
        int x = ((i * 40 - (g_.steps * 8)) % 240 + 240) % 240 - 30;
        cv.fillRect(x, 248, 24, 3, gfx::hex(0x3A403A));
      }
      int sway = (g_.steps % 2) ? 2 : -2;
      cv.sprite(*creatureSprite(s.species, form, g_.wobbles > 3 ? 2 : 0), 36 + sway, 150);
      cv.fillRectA(0, 40, 86, 280, th::moon, ptr_.down && ptr_.x < 86 ? 30 : 8);
      cv.fillRectA(86, 40, 86, 280, th::moon, ptr_.down && ptr_.x >= 86 ? 30 : 8);
      cv.textCenter(F14B, 43, 290, "LEFT", th::fog);
      cv.textCenter(F14B, 129, 290, "RIGHT", th::fog);
      cv.text(F12B, 6, 314, "Left, right. Calmly.", th::fog);
      break;
    }
  }
}
