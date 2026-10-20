// App drawing for the 466 x 466 round AMOLED (M5Stack StopWatch build):
// every screen, overlay and minigame, plus hitAt() with the same geometry.
//
// The idea: the whole circle is a lens on the cryptid's world. In the DEN the
// scene fills the circle and the creature stands on the horizon; the name,
// needs, hint and one action button sit on the dark ground below it, and the
// time and four tab pills sit in the sky above. Holds (incubate, cuddle, dig
// out, power off) draw as rings, because rings suit a round screen. Nessie's
// loch level and the minigame timer run around the rim.
#include "target.h"
#if CRYPT_ROUND
#include <cmath>
#include <cstdio>
#include <cstring>

#include "app.h"
#include "art.h"
#include "data.h"
#include "layout.h"
#include "theme.h"

using namespace cryptlings;
using gfx::Color;

namespace {
const char* phaseName(float p) {
  static const char* n[8] = {"New moon", "Waxing crescent", "First quarter", "Waxing gibbous",
                             "Full moon", "Waning gibbous", "Last quarter", "Waning crescent"};
  return n[(int)(p * 8.0f + 0.5f) % 8];
}
void upper(char* s) { for (; *s; ++s) if (*s >= 'a' && *s <= 'z') *s -= 32; }

bool in(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}
bool inCircle(int x, int y, int cx, int cy, int r) {
  int dx = x - cx, dy = y - cy;
  return dx * dx + dy * dy <= r * r;
}

struct Civil { unsigned mo, d; int h, m; };
Civil civil(int64_t local) {
  int sec = (int)((local % 86400 + 86400) % 86400);
  int64_t z = (local >= 0 ? local : local - 86399) / 86400 + 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  unsigned doe = (unsigned)(z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  Civil c;
  c.d = doy - (153 * mp + 2) / 5 + 1;
  c.mo = mp < 10 ? mp + 3 : mp - 9;
  c.h = sec / 3600;
  c.m = (sec / 60) % 60;
  return c;
}

// Tab pill i: left edge.
int tabX(int i) { return L::CX + (i * 2 - 3) * L::TAB_STEP / 2 - L::TAB_W / 2; }

// Paint a vertical fade of `c` from alpha a0 at y0 to a1 at y1, in 4 px bands.
void fade(gfx::Canvas& cv, int y0, int y1, Color c, int a0, int a1) {
  for (int y = y0; y < y1; y += 4) {
    int a = a0 + (a1 - a0) * (y - y0) / (y1 - y0);
    if (a > 0) cv.fillRectA(0, y, gfx::W, 4, c, (uint8_t)(a > 255 ? 255 : a));
  }
}

// An arc of dots from angle a0 to a1 (radians, 0 = 12 o'clock, clockwise).
void arc(gfx::Canvas& cv, int cx, int cy, int r, int thick, float a0, float a1, Color c, uint8_t alpha = 255) {
  if (a1 <= a0) return;
  int steps = (int)((a1 - a0) * r / 2.5f) + 1;
  for (int i = 0; i <= steps; ++i) {
    float a = a0 + (a1 - a0) * i / steps;
    int x = cx + (int)std::lround(r * std::sin(a)), y = cy - (int)std::lround(r * std::cos(a));
    cv.fillCircle(x, y, thick, c, alpha);
  }
}

// Draw a sprite so its visible pixels are centred on (cx, cy).
void spriteCentered(gfx::Canvas& cv, const Sprite& s, int cx, int cy, const gfx::Tint& t = gfx::Tint()) {
  cv.sprite(s, cx - s.ox - s.w / 2, cy - s.oy - s.h / 2, t);
}
}  // namespace

// ---------------------------------------------------------------------------
void App::render() {
  cv.noClip();
  switch (scr_) {
    case Scr::BOOT: drawBoot(); break;
    case Scr::TRAY: drawTray(); break;
    case Scr::NAME: drawName(); break;
    case Scr::GAME: drawGame(); break;
    case Scr::MAIN:
      cv.fill(th::bg);
      if (tab_ == Tab::DEN) drawDen();
      else if (tab_ == Tab::CARE) drawCare();
      else if (tab_ == Tab::FILE) drawFile();
      else drawMore();
      {
        int i = (int)tab_;
        int maxS = contentH_[i] - L::VIEW_H;
        if (maxS < 0) maxS = 0;
        if (i && scroll_[i] > maxS) { scroll_[i] = maxS; dirty_ = true; }
      }
      cv.noClip();
      if (tab_ != Tab::DEN) {
        // Lists fade out under the tabs and into the narrowing bottom of the circle.
        fade(cv, L::CONTENT_Y, L::CONTENT_Y + 16, th::bg, 220, 0);
        fade(cv, 392, gfx::H, th::bg, 0, 255);
      }
      fade(cv, 0, L::CONTENT_Y, th::bg, 230, tab_ == Tab::DEN ? 40 : 230);
      drawStatus();
      drawTabs();
      break;
  }
  cv.noClip();
  drawOverlay();
  drawToast();
  if (powerHold_ > 0) {
    cv.fillRectA(0, 0, gfx::W, gfx::H, th::bg, 248);
    arc(cv, L::CX, L::CY, 222, 2, 0, 6.2832f, th::line);
    arc(cv, L::CX, L::CY, 222, 4, 0, 6.2832f * powerHold_, th::lantern);
    cv.textCenter(FHead, L::CX, 200, "Keep holding", th::moon);
    cv.textCenter(FHead, L::CX, 236, "to power off", th::moon);
    cv.textCenter(FLabel, L::CX, 286, "Your pet is saved.", th::fog);
  }
}

void App::renderPowerOff() {
  save(true);
  cv.noClip();
  cv.fill(th::bg);
  for (int i = 0; i < 5; ++i) cv.fillEllipse(70 + i * 82, 360 + (i % 2) * 18, 90, 14, th::fog, 40);
  cv.moon(L::CX, 128, 40, Pet::moonPhase(now_), th::moon, th::line);
  cv.textCenter(FTitle, L::CX, 222, "Powered off", th::moon);
  cv.textWrap(FBody, 93, 262, 280, set_.hasPet ? "Your cryptid is curled up in its burrow. Press the power button to wake it."
                                               : "Press the power button to wake.", th::text, 4, 0, true);
}

// ---- chrome ----------------------------------------------------------------
void App::drawStatus() {
  int64_t l = (int64_t)now_ + P.utcOffset(now_);
  Civil c = civil(l);
  char tbuf[16], bbuf[12];
  std::snprintf(tbuf, sizeof(tbuf), "%d:%02d %s", c.h % 12 == 0 ? 12 : c.h % 12, c.m, c.h < 12 ? "AM" : "PM");
  int bat = P.batteryPercent();
  bbuf[0] = 0;
  if (bat >= 0) std::snprintf(bbuf, sizeof(bbuf), "%d%%", bat);
  int tw = gfx::Canvas::textWidth(FLabel, tbuf), bw = bbuf[0] ? gfx::Canvas::textWidth(FLabel, bbuf) + 18 : 0;
  int total = 20 + tw + (bw ? 14 + bw : 0);
  int x = L::CX - total / 2;
  int y = L::STATUS_BASE;
  cv.moon(x + 7, y - 6, 7, Pet::moonPhase(now_), th::moon, th::line);
  x += 20;
  cv.text(FLabel, x, y, tbuf, th::fog);
  x += tw + 14;
  if (bw) {
    Color bc = bat < 20 ? th::ember : th::fog;
    cv.rect(x, y - 14, 9, 15, bc);
    cv.fillRect(x + 3, y - 16, 3, 2, bc);
    int fill = bat * 11 / 100;
    cv.fillRect(x + 2, y - 2 - fill, 5, fill, bat < 20 ? th::ember : th::lantern);
    cv.text(FLabel, x + 16, y, bbuf, bc);
  }
  if (!P.clockVerified()) cv.fillCircle(L::CX + total / 2 + 10, y - 6, 3, th::ember);   // clock not set (see MORE)
}

void App::drawTabs() {
  static const char* names[4] = {"Den", "Care", "File", "More"};
  for (int i = 0; i < 4; ++i) {
    int x = tabX(i);
    bool on = (int)tab_ == i;
    if (on) cv.fillRound(x, L::TABS_Y, L::TAB_W, L::TABS_H, 16, th::manila);
    else cv.roundRect(x, L::TABS_Y, L::TAB_W, L::TABS_H, 16, th::line, 2);
    cv.textCenter(FHead, x + L::TAB_W / 2, L::TABS_Y + 24, names[i], on ? th::ink : th::fog);
  }
}

void App::drawPaperCard(int x, int y, int w, int h) {
  cv.fillRectA(x + 3, y + 5, w, h, gfx::hex(0x000000), 140);
  cv.fillRound(x, y, w, h, 8, th::paper);
}

void App::drawButton(int x, int y, int w, int h, const char* label, bool primary, const void* icon) {
  if (primary) cv.fillRound(x - 3, y - 3, w + 6, h + 6, h / 2 + 3, th::lantern);
  cv.fillRound(x, y, w, h, h / 2, th::paper);
  cv.roundRect(x, y, w, h, h / 2, th::ink, 2);
  int lw = gfx::Canvas::textWidth(FBodyB, label);
  int base = y + h / 2 + 7;
  if (icon) {
    int total = 28 + 10 + lw;
    int ix = x + (w - total) / 2;
    cv.sprite(*(const Sprite*)icon, ix, y + (h - 28) / 2);
    cv.text(FBodyB, ix + 38, base, label, th::ink);
  } else {
    cv.textCenter(FBodyB, x + w / 2, base, label, th::ink);
  }
}

void App::drawHoldButton(int x, int y, int w, int h, const char* label, int id) {
  int r = h / 2;
  cv.fillRound(x, y, w, h, r, th::paper);
  if (holdId_ == id && holdP_ > 0) {
    int fw = (int)(w * (holdP_ > 1 ? 1 : holdP_));
    cv.clip(x, y, fw, h);
    cv.fillRound(x, y, w, h, r, th::lantern);
    cv.noClip();
  }
  cv.roundRect(x, y, w, h, r, th::lantern, 3);
  cv.textCenter(FBodyB, x + w / 2, y + h / 2 + 7, label, th::ink);
}

void App::drawRing(int cx, int cy, int r, float p, Color c) {
  if (p <= 0) return;
  arc(cv, cx, cy, r, 4, 0, 6.2832f * (p > 1 ? 1 : p), c);
}

void App::drawToast() {
  if (!toast_[0] || !toastUntil_) return;
  // On the round screen a toast would sit on top of an overlay's choices; it waits.
  if (ov_ == Ov::PANTS || ov_ == Ov::CLOCK || ov_ == Ov::CONFIRM_RESET || ov_ == Ov::HIKER || ov_ == Ov::REVEAL) return;
  const int x = 83, w = 300;
  int lines = gfx::Canvas::wrapLines(FLabel, w - 24, toast_);
  if (lines > 4) lines = 4;
  int h = lines * (FLabel.line + 2) + 20;
  int y = 92;
  drawPaperCard(x, y, w, h);
  cv.fillRectA(L::CX - 30, y - 6, 60, 14, th::manila, 190);   // tape
  cv.textWrap(FLabel, x + 12, y + 10 + FLabel.ascent, w - 24, toast_, th::ink, 2, 4, true);
}

// ---- creature + scene ---------------------------------------------------------
void App::drawCreature(int x, int y, int form, int mood, uint8_t alpha, int div) {
  const State& s = pet_.state();
  bool blink = mood != 3 && (ms_ % 4100) < 140;
  int m = blink ? 4 : mood;
  if (form == 5) m = mood == 3 ? 3 : 1;   // the blurry form is always mopey
  gfx::Tint t;
  t.a = alpha;
  if (scr_ == Scr::MAIN && tod() == 3) { t.r = 205; t.g = 205; t.b = 220; }
  cv.sprite(*creatureSprite(s.species, form, m), x, y, t, div);
  if ((Form)s.form == Form::HOAX && form == 4 && div == 1) {   // the zipper gives it away
    for (int i = 0; i < 52; i += 6) cv.fillRect(x + 97, y + 80 + i, 4, 3, th::moon);
  }
}

// The DEN backdrop: the scene, the moon, fireflies, the creature and fog.
void App::drawPhoto(int px, int py, bool live) {
  (void)px; (void)py;
  const State& s = pet_.state();
  int td = tod();
  cv.sprite(*sceneSprite(s.species, td), 0, 0);
  if (td != 3) cv.moon(112, 140, 20, Pet::moonPhase(now_), td == 2 ? th::fog : th::moon, gfx::hex(0x1E2436));
  if (live && (td == 0 || td == 1)) {
    for (int i = 0; i < 6; ++i) {
      float t = ms_ / 1000.0f;
      int fx = 110 + i * 50 + (int)(std::sin(t * 0.7f + i * 2) * 18);
      int fy = 190 + (int)(std::cos(t * 0.9f + i) * 26) - (i % 2) * 30;
      uint8_t a = (uint8_t)(120 + 110 * std::sin(t * 2 + i));
      cv.fillCircle(fx, fy, 3, th::firefly, a);
    }
  }
  int form = formIdx();
  bool hidden = s.hideReadyAt && now_ + (uint32_t)(pet_.tuning().hideCooldownMin * 60) - s.hideReadyAt < 20;
  int bob = live && moodIdx() != 3 ? ((ms_ / 420) % 2 ? -2 : 0) : 0;
  if (s.hibernating) {
    cv.fillEllipse(L::CX, 276, 96, 32, gfx::hex(0x3A2E22));
    cv.fillEllipse(L::CX, 268, 76, 24, gfx::hex(0x4A3A2A));
    cv.text(FHand, 290, 220 - (int)((ms_ / 300) % 6) * 2, "z", th::moon);
  } else {
    drawCreature(L::PET_X, L::PET_Y + bob, form, moodIdx(), hidden ? 50 : 255);
    if (moodIdx() == 3 && !hidden) {
      int zz = (int)((ms_ / 250) % 12);
      cv.text(FHand, 318 + zz, 140 - zz * 2, "z", th::moon, (uint8_t)(255 - zz * 18));
    }
  }
  // Fog drifts in front.
  int drift = (int)((ms_ / 90) % 560);
  cv.fillEllipse(-60 + drift, 286, 120, 12, th::fog, 55);
  cv.fillEllipse(520 - (drift * 7 / 10) % 600, 296, 140, 10, th::fog, 40);
  if (holdId_ != 0 && holdP_ > 0 && scr_ == Scr::MAIN) drawRing(L::PET_CX, L::PET_CY, L::PET_R, holdP_, th::lantern);
}

// ---- DEN ----------------------------------------------------------------------
void App::drawDen() {
  const State& s = pet_.state();
  drawPhoto(0, 0, true);
  fade(cv, 290, gfx::H, th::bg, 0, 210);   // the ground darkens under the controls

  // Stamp (how believable it is) in the sky, upper right.
  int form = (int)s.form;
  const char* sw = STAMP_WORD[form];
  int sw_w = gfx::Canvas::textWidth(FLabel, sw) + 14;
  Color sc = form == 3 ? th::stamp : form == 4 ? th::confirmed : th::stampBlue;
  int sx = 392 - sw_w, sy = 100;
  cv.fillRectA(sx, sy, sw_w, 26, th::paper, 210);
  cv.rect(sx, sy, sw_w, 26, sc, 2);
  cv.text(FLabel, sx + 7, sy + 19, sw, sc);

  // Name + stage stamp, centred together.
  char st[24];
  std::snprintf(st, sizeof(st), "%s", form == (int)Form::BLURRY ? "BLURRY" : STAGE_NAME[form]);
  upper(st);
  int nw = gfx::Canvas::textWidth(FHand, set_.name);
  int stw = gfx::Canvas::textWidth(FLabel, st) + 12;
  int x0 = L::CX - (nw + 12 + stw) / 2;
  cv.text(FHand, x0, L::NAME_BASE, set_.name, th::moon);
  int stx = x0 + nw + 12;
  cv.rect(stx, L::NAME_BASE - 20, stw, 24, th::ember, 2);
  cv.text(FLabel, stx + 6, L::NAME_BASE - 2, st, th::ember);

  // Needs: icon + 5 pips each.
  static const Icon icons[5] = {Icon::BELLY, Icon::CHEER, Icon::REST, Icon::MYST, Icon::TIDY};
  static const Color cols[5] = {th::lantern, th::pink, th::dusk, th::loch, th::moss};
  bool egg = (Form)s.form == Form::EGG;
  for (int i = 0; i < 5; ++i) {
    int cx = L::STATS_X0 + L::STATS_STEP / 2 + i * L::STATS_STEP;
    float v = s.stat[i];
    gfx::Tint t;
    if (egg) t.a = 90;
    cv.sprite(*iconSprite(icons[i]), cx - 14, L::STATS_Y, t);
    int on = egg ? 0 : (int)((v + 19.99f) / 20.0f);
    for (int k = 0; k < 5; ++k) {
      int x = cx - 24 + k * 10;
      Color c = k < on ? (v < 25 ? th::ember : cols[i]) : th::panel;
      cv.fillRect(x, L::STATS_Y + 34, 7, 11, c);
      if (k >= on) cv.rect(x, L::STATS_Y + 34, 7, 11, th::line);
    }
  }
  // Nessie: the loch level runs up the left rim.
  if ((Species)s.species == Species::NESSIE && !egg) {
    const float a0 = 3.4f, a1 = 5.6f;   // lower left to upper left
    arc(cv, L::CX, L::CY, 222, 3, a0, a1, th::panel);
    float lvl = s.loch < 0 ? 0 : (s.loch > 100 ? 100 : s.loch);
    arc(cv, L::CX, L::CY, 222, 3, a1 - (a1 - a0) * lvl / 100.0f, a1, s.loch < 30 ? th::ember : th::loch);
  }
  // Hint
  char hb[48];
  cv.textCenter(FLabel, L::CX, L::HINT_BASE, hintLine(hb, sizeof(hb)), th::lantern);
  // The one action button (CARE is a tab away).
  int a = bestAction();
  const char* lab = "PAT";
  Icon ic = Icon::CHEER;
  switch (a) {
    case A_WARM: lab = "WARM"; ic = Icon::LIGHTS; break;
    case A_FEED: lab = "FEED"; ic = Icon::BELLY; break;
    case A_LIGHTS: lab = asleepNow() ? "SHH" : "NAP"; ic = Icon::REST; break;
    case A_CLEAN: lab = "CLEAN"; ic = Icon::TIDY; break;
    case A_PLAY: lab = "PLAY"; ic = Icon::PLAY; break;
    case A_HIDE: lab = "HIDE"; ic = Icon::MYST; break;
  }
  drawButton(L::BEST_X, L::BEST_Y, L::BEST_W, L::BEST_H, lab, true, iconSprite(ic));
}

// ---- CARE ---------------------------------------------------------------------
void App::drawCare() {
  const State& s = pet_.state();
  const SpeciesText& T = SPECIES_TEXT[s.species & 3];
  cv.fill(th::bg2);
  cv.clip(0, L::CONTENT_Y, gfx::W, gfx::H - L::CONTENT_Y);
  int ids[12];
  int n = careRows(ids, 12);
  char sub[64];
  for (int i = 0; i < n; ++i) {
    int y = L::CONTENT_Y + 8 + i * (L::ROW_H + L::ROW_GAP) - scroll_[1];
    const char* title = "";
    Icon ic = Icon::BELLY;
    bool dim = false;
    sub[0] = 0;
    switch (ids[i]) {
      case A_FEED: title = "FEED"; std::snprintf(sub, sizeof(sub), "%s", T.food); break;
      case A_TREAT: title = "TREAT"; ic = Icon::TREAT;
        std::snprintf(sub, sizeof(sub), "%d left tonight", s.treatsLeft); dim = s.treatsLeft == 0; break;
      case A_PLAY: title = "PLAY"; ic = Icon::PLAY;
        std::snprintf(sub, sizeof(sub), "%s, 20 sec", T.game); dim = now_ < s.dizzyUntil; break;
      case A_HIDE: title = "HIDE"; ic = Icon::MYST;
        std::snprintf(sub, sizeof(sub), "%s", now_ < s.hideReadyAt ? "Still hidden for now" : "Raises Mystique");
        dim = now_ < s.hideReadyAt; break;
      case A_CLEAN: title = "CLEAN"; ic = Icon::TIDY; std::snprintf(sub, sizeof(sub), "Burrs and fog out"); break;
      case A_LIGHTS: title = "LIGHTS"; ic = Icon::LIGHTS; {
          float h = pet_.localHour(now_);
          std::snprintf(sub, sizeof(sub), "%s", (h >= 5.5f && h < 18.0f) ? "Tuck in till dusk" : "One short nap");
        } break;
      case A_LAMP: title = lampOn_ ? "LAMP: ON" : "LAMP"; ic = Icon::LAMP;
        std::snprintf(sub, sizeof(sub), "%s", lampOn_ ? "Tap to switch off" : "He loves it. Briefly.");
        dim = now_ < s.dizzyUntil; break;
      case A_FORAGE: title = "FORAGE"; ic = Icon::FORAGE; std::snprintf(sub, sizeof(sub), "Wild berries"); break;
      case A_LOCH: title = "TOP UP LOCH"; ic = Icon::LOCH;
        std::snprintf(sub, sizeof(sub), "Loch level %d%%", (int)s.loch); break;
      case A_WALK: title = "WALK"; ic = Icon::WALK;
        std::snprintf(sub, sizeof(sub), "%s", s.walked ? "Walked tonight" : hasMotion_ ? "Take him out, for real" : "Evening Stroll");
        dim = s.walked; break;
      case A_PANTS: title = "PANTS"; ic = Icon::PANTS;
        std::snprintf(sub, sizeof(sub), "%s", s.pantsDone ? "Dressed for tonight" : "Pick tonight's pair");
        dim = s.pantsDone; break;
    }
    cv.fillRound(L::ROW_X, y, L::ROW_W, L::ROW_H, 12, dim ? th::manilaDk : th::paper);
    cv.roundRect(L::ROW_X, y, L::ROW_W, L::ROW_H, 12, th::ink, 2);
    cv.sprite(*iconSprite(ic), L::ROW_X + 16, y + 16);
    cv.text(FBodyB, L::ROW_X + 58, y + 26, title, th::ink);
    cv.textWrap(FLabel, L::ROW_X + 58, y + 49, L::ROW_W - 70, sub, th::sepia, 0, 1);
  }
  contentH_[1] = n * (L::ROW_H + L::ROW_GAP) + 16;
}

// ---- FILE ---------------------------------------------------------------------
void App::drawFile() {
  const State& s = pet_.state();
  const SpeciesText& T = SPECIES_TEXT[s.species & 3];
  cv.fill(th::manila);
  cv.clip(0, L::CONTENT_Y, gfx::W, gfx::H - L::CONTENT_Y);
  const int x = L::COL_X, w = L::COL_W;
  int y = L::CONTENT_Y + 14 - scroll_[2];
  y += FHead.ascent;
  cv.textCenter(FHead, L::CX, y, T.name, th::ink);
  y += 8;
  y = cv.textWrap(FLabel, x, y + FLabel.ascent, w, T.origin, th::sepia, 2, 0, true);
  y += 6;
  y = cv.textWrap(FBody, x, y + FBody.ascent, w, T.bio, th::ink, 3);
  auto label = [&](const char* t) {
    y += 8;
    for (int i = 0; i < w; i += 6) cv.fillRect(x + i, y, 3, 2, th::sepia);
    y += 8 + FLabel.ascent;
    cv.text(FLabel, x, y, t, th::sepia);
    y += 10;
  };
  label("KNOWN QUIRK");
  y = cv.textWrap(FBody, x, y + FBody.ascent, w, T.quirk, th::ink, 3);
  label("PHOTO RECORD");
  static const int forms[5] = {0, 1, 2, 3, 4};
  const int cw = 96, ch = 120, gap = 6;
  for (int i = 0; i < 5; ++i) {
    int f = forms[i];
    if (i == 4) f = s.form >= (uint8_t)Form::LEGEND ? s.form : 4;
    bool got = s.stagePhotos & (1u << f);
    int col = i < 3 ? i : i - 3;
    int tx = i < 3 ? x + col * (cw + gap) : x + (cw + gap) / 2 + col * (cw + gap);
    int ty = y + (i / 3) * (ch + gap);
    cv.fillRect(tx, ty, cw, ch, th::paper);
    cv.fillRect(tx + 5, ty + 5, cw - 10, 88, got ? th::bg2 : gfx::hex(0x3C3528));
    if (got) {
      cv.clip(tx + 5, ty + 5, cw - 10, 88);
      int fi = f == (int)Form::HOAX ? 4 : f;
      gfx::Tint t;
      cv.sprite(*creatureSprite(s.species, fi, 1), tx - 2, ty - 4, t, 2);
      cv.clip(0, L::CONTENT_Y, gfx::W, gfx::H - L::CONTENT_Y);
    }
    char nm[16];
    std::snprintf(nm, sizeof(nm), "%s", got ? STAGE_NAME[f] : "???");
    if (f == (int)Form::BLURRY && got) std::snprintf(nm, sizeof(nm), "BLURRY");
    upper(nm);
    cv.textCenter(FLabel, tx + cw / 2, ty + ch - 8, nm, th::ink);
  }
  y += 2 * (ch + gap) + 4;
  int found = 0;
  for (int i = 0; i < 16; ++i) if (s.evidence[s.species][i]) ++found;
  char lb[32];
  std::snprintf(lb, sizeof(lb), "EVIDENCE %d/16", found);
  label(lb);
  if (!found) {
    y = cv.textWrap(FBody, x, y + FBody.ascent, w, "Nothing yet. Check in at night, play, and keep up good care.", th::ink, 3);
  }
  for (int i = 0; i < 16; ++i) {
    uint8_t c = s.evidence[s.species][i];
    if (!c) continue;
    char line[80];
    std::snprintf(line, sizeof(line), "%s", T.evidence[i]);
    y = cv.textWrap(FBody, x, y + FBody.ascent, w, line, th::ink, 2);
    if (c >= 5) std::snprintf(line, sizeof(line), "CONFIRMED x%d", c);
    else if (c > 1) std::snprintf(line, sizeof(line), "%s x%d", TIER_NAME[(int)Pet::evidenceTier(i)], c);
    else std::snprintf(line, sizeof(line), "%s", TIER_NAME[(int)Pet::evidenceTier(i)]);
    cv.text(FLabel, x + 10, y + FLabel.ascent - 4, line, th::stamp);
    y += FLabel.line + 6;
  }
  if (s.form >= (uint8_t)Form::LEGEND) {
    label("WHEN IT IS READY");
    y = cv.textWrap(FBody, x, y + FBody.ascent, w,
                    s.restless ? "Something out there is calling. You can let it go, and it will leave you a souvenir."
                               : "Adults can be released into the fog. Nothing is lost: the photos stay here.",
                    th::ink, 3);
    int by = y + 10;
    drawHoldButton(x, by, w, 56, "HOLD TO RELEASE", 4);
    y = by + 56 + 14;
    // hitAt finds the button from contentH_: by = contentH_ - 70 + CONTENT_Y - scroll
    contentH_[2] = y + scroll_[2] - L::CONTENT_Y;
  } else {
    y += 14;
    contentH_[2] = y + scroll_[2] - L::CONTENT_Y;
  }
}

// ---- MORE (almanac + settings) -----------------------------------------------
void App::drawMore() {
  cv.fill(th::bg2);
  cv.clip(0, L::CONTENT_Y, gfx::W, gfx::H - L::CONTENT_Y);
  int y = L::CONTENT_Y + 12 - scroll_[3];
  float p = Pet::moonPhase(now_);
  cv.textCenter(FHead, L::CX, y + FHead.ascent, "Almanac", th::moon);
  cv.moon(130, y + 84, 30, p, th::moon, th::line);
  int tx = 178;
  cv.text(FLabel, tx, y + 66, phaseName(p), th::text);
  float toFull = p <= 0.5f ? (0.5f - p) : (1.5f - p);
  int nights = (int)(toFull * 29.53f + 0.5f);
  char buf[48];
  if (Pet::nearFullMoon(now_, 18)) std::snprintf(buf, sizeof(buf), "Full tonight!");
  else std::snprintf(buf, sizeof(buf), "Full in %d nights", nights);
  cv.text(FLabel, tx, y + 90, buf, th::lantern);
  cv.text(FLabel, tx, y + 114, "(grows faster)", th::fog);
  cv.textCenter(FLabel, L::CX, y + 150, "Wakes 6 pm, sleeps 7 am", th::text);
  cv.textCenter(FLabel, L::CX, y + 174, "Evolves 8 to 11 pm", th::text);
  cv.textCenter(FLabel, L::CX, y + 198, "Quiet 11 pm to 7 am", th::text);
  cv.textCenter(FLabel, L::CX, y + 222, P.clockVerified() ? "Clock set" : "Clock: set it below",
                P.clockVerified() ? th::moss : th::ember);
  static const char* sounds[3] = {"On", "Chimes only", "Off"};
  static const char* bright[3] = {"Low", "Medium", "High"};
  static const char* tos[3] = {"30 seconds", "1 minute", "2 minutes"};
  const char* titles[6] = {"SOUND", "BRIGHTNESS", "SCREEN OFF AFTER", "BATTERY SAVER", "SET CLOCK", "START OVER"};
  const char* subs[6] = {sounds[set_.sound % 3], bright[set_.brightness % 3], tos[set_.timeout % 3],
                         set_.saver ? "On: deep rest" : "Off", "Adjust date and time", "New egg tray"};
  int y0 = L::CONTENT_Y + L::ALMANAC_H - scroll_[3];
  for (int i = 0; i < 6; ++i) {
    int ry = y0 + i * (L::ROW_H + L::ROW_GAP);
    cv.fillRound(L::ROW_X, ry, L::ROW_W, L::ROW_H, 12, th::panel);
    cv.roundRect(L::ROW_X, ry, L::ROW_W, L::ROW_H, 12, th::line, 2);
    cv.text(FBodyB, L::ROW_X + 20, ry + 26, titles[i], th::text);
    cv.text(FLabel, L::ROW_X + 20, ry + 49, subs[i], i == 5 ? th::ember : th::lantern);
  }
  int yend = y0 + 6 * (L::ROW_H + L::ROW_GAP) + 10;
  yend = cv.textWrap(FLabel, L::COL_X, yend + FLabel.ascent, L::COL_W,
                     "CRYPTLINGS. A field journal for creatures nobody believes in. Be kind to them.", th::dim, 2, 0, true);
  contentH_[3] = yend + 16 + scroll_[3] - L::CONTENT_Y;
}

// ---- BOOT ---------------------------------------------------------------------
void App::drawBoot() {
  cv.fill(th::bg);
  cv.fillRound(73, 66, 320, 330, 8, th::manila);
  cv.fillRound(95, 52, 110, 24, 8, th::manila);   // folder tab
  Civil c = civil((int64_t)now_ + P.utcOffset(now_));
  static const char* mon[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  char subj[24], stat[24], filed[28], moonl[32], caseNo[24];
  bool hasPet = set_.hasPet;
  char sp[16];
  std::snprintf(sp, sizeof(sp), "%s", hasPet ? SPECIES_TEXT[pet_.state().species & 3].shortName : "UNKNOWN");
  upper(sp);
  std::snprintf(subj, sizeof(subj), "SUBJ: %s", sp);
  std::snprintf(stat, sizeof(stat), "STAT: %s", hasPet ? STAMP_WORD[pet_.state().form % 7] : "UNVERIFIED");
  std::snprintf(filed, sizeof(filed), "FILED: %s %u %d:%02d%s", mon[(c.mo + 11) % 12], c.d, c.h % 12 == 0 ? 12 : c.h % 12, c.m,
                c.h < 12 ? "A" : "P");
  char ph[20];
  std::snprintf(ph, sizeof(ph), "%s", phaseName(Pet::moonPhase(now_)));
  upper(ph);
  std::snprintf(moonl, sizeof(moonl), "MOON: %s", ph);
  std::snprintf(caseNo, sizeof(caseNo), "CASE FILE #%04u", set_.caseNo);
  const char* lines[] = {"DEPT. OF ODD FAUNA", "FIELD OFFICE 7", "", caseNo, subj, stat, filed, moonl, "",
                         "NOTES: Something is", "in the woods. It is", "probably fine."};
  int n = sizeof(lines) / sizeof(lines[0]);
  int chars = (int)((ms_ - bootStart_) / 20);
  int y = 100;
  for (int i = 0; i < n && chars > 0; ++i) {
    char buf[32];
    int len = (int)std::strlen(lines[i]);
    int k = chars < len ? chars : len;
    std::memcpy(buf, lines[i], k);
    buf[k] = 0;
    cv.text(FLabel, 97, y, buf, th::ink);
    chars -= len + 4;
    y += 22;
  }
  if (chars > 0) {
    cv.rect(L::CX - 80, 354, 160, 32, th::stampBlue, 3);
    cv.textCenter(FLabel, L::CX, 377, hasPet ? "CASE OPEN" : "UNVERIFIED", th::stampBlue);
    if ((ms_ / 600) % 2) cv.textCenter(FHand, L::CX, 434, "Tap to open the file", th::fog);
  }
}

// ---- TRAY + NAME ----------------------------------------------------------------
void App::drawTray() {
  cv.fill(th::bg);
  drawStatus();
  const SpeciesText& T = SPECIES_TEXT[trayPick_];
  char k[24];
  std::snprintf(k, sizeof(k), "CASE FILE #%04u", set_.caseNo);
  cv.textCenter(FLabel, L::CX, 62, k, th::fog);
  cv.textCenter(FTitle, L::CX, 104, "Specimen tray", th::moon);
  // nest glow + the egg, held = ring fills
  cv.fillEllipse(L::CX, L::EGG_CY + 66, 86, 16, th::lantern, 40);
  int bob = (ms_ / 500) % 2 ? -2 : 0;
  spriteCentered(cv, *creatureSprite(trayPick_, 0, (ms_ / 3000) % 3 == 0 ? 0 : 1), L::EGG_CX, L::EGG_CY + bob);
  arc(cv, L::EGG_CX, L::EGG_CY, L::EGG_RING, 2, 0, 6.2832f, th::line);
  if (holdId_ == 1 && holdP_ > 0) drawRing(L::EGG_CX, L::EGG_CY, L::EGG_RING, holdP_, th::lantern);
  cv.textCenter(FTitle, L::ARROW_L, L::EGG_CY + 14, "<", th::lantern);
  cv.textCenter(FTitle, L::ARROW_R, L::EGG_CY + 14, ">", th::lantern);
  for (int i = 0; i < 4; ++i) cv.fillCircle(L::CX - 27 + i * 18, 326, 4, i == trayPick_ ? th::lantern : th::line);
  cv.textCenter(FHead, L::CX, 358, T.name, th::text);
  if (pet_.state().raisedMask & (1u << trayPick_)) {
    cv.fillRectA(306, 134, 70, 26, th::manila, 220);
    cv.text(FLabel, 316, 153, "SEEN", th::stamp);
  }
  cv.textWrap(FLabel, 83, 386, 300, T.eggHint, th::fog, 0, 2, true);
  if ((ms_ / 700) % 3) cv.textCenter(FBodyB, L::CX, 440, "HOLD THE EGG", th::lantern);
}

void App::drawName() {
  cv.fill(th::bg);
  drawStatus();
  const SpeciesText& T = SPECIES_TEXT[pet_.state().species & 3];
  cv.textCenter(FLabel, L::CX, 66, "NAME THE SPECIMEN", th::fog);
  cv.fillEllipse(L::CX, 250, 86, 16, th::lantern, 40);
  spriteCentered(cv, *creatureSprite(pet_.state().species, 0, 0), L::CX, 184 + ((ms_ / 500) % 2 ? -2 : 0));
  cv.textCenter(FTitle, L::CX, 312, T.names[nameIdx_ % 6], th::moon);
  cv.textCenter(FLabel, L::CX, 344, "Written on the egg", th::dim);
  cv.textCenter(FLabel, L::CX, 366, "in pencil. Lightly.", th::dim);
  drawButton(L::NAME_BTN1_X, L::NAME_BTN_Y, L::NAME_BTN_W, L::NAME_BTN_H, "ANOTHER", false);
  drawButton(L::NAME_BTN2_X, L::NAME_BTN_Y, L::NAME_BTN_W, L::NAME_BTN_H, "USE IT", true);
}

// ---- overlays -------------------------------------------------------------------
void App::drawOverlay() {
  if (ov_ == Ov::NONE) return;
  const State& s = pet_.state();
  cv.fillRectA(0, 0, gfx::W, gfx::H, th::bg, 215);
  switch (ov_) {
    case Ov::REVEAL: {
      uint32_t t = ms_ - revealStart_;
      const int px = 113, py = 50, pw = 240, ph = 280;
      cv.fillRect(px + 4, py + 6, pw, ph, gfx::hex(0x000000));
      cv.fillRect(px, py, pw, ph, th::paper);
      cv.vgradient(px + 10, py + 10, pw - 20, 220, gfx::hex(0x0D1424), gfx::hex(0x1D2A22), 8);
      cv.clip(px + 10, py + 10, pw - 20, 220);
      int f = revealForm_ == (uint8_t)Form::HOAX ? 4 : revealForm_;
      int mood = f == 5 ? 1 : 0;
      cv.sprite(*creatureSprite(s.species, f, mood), L::CX - 100, py + 22);
      int step = (int)(t / 300);   // develop: paper fades out over 2.4 s in 8 steps
      if (step < 8) cv.fillRectA(px + 10, py + 10, pw - 20, 220, th::paper, (uint8_t)(255 - step * 32));
      cv.noClip();
      bool adult = revealForm_ >= (uint8_t)Form::LEGEND;
      if (t > 2600) {
        char w[24];
        std::snprintf(w, sizeof(w), "%s", STAGE_NAME[revealForm_]);
        upper(w);
        int ww = gfx::Canvas::textWidth(FHead, w) + 28;
        if (ww > pw - 20) ww = pw - 20;
        cv.rect(L::CX - ww / 2, py + 236, ww, 36, th::stamp, 3);
        cv.textCenter(FHead, L::CX, py + 263, w, th::stamp);
      } else {
        cv.textCenter(FHand, L::CX, py + 262, "Developing...", th::sepia);
      }
      const char* cap = "";
      char cb[64];
      if (revealForm_ == (uint8_t)Form::LEGEND) cap = "A true Legend.";
      else if (revealForm_ == (uint8_t)Form::BLURRY) cap = "Still yours. Still out there.";
      else if (revealForm_ == (uint8_t)Form::HOAX) cap = "Debunked. Lovingly.";
      else { std::snprintf(cb, sizeof(cb), "Now a %s.", STAGE_NAME[revealForm_]); cap = cb; }
      cv.textCenter(FHand, L::CX, 362, cap, th::moon);
      if (adult && t > 2600) {
        float careAvg = s.weight > 0 ? s.careW / s.weight : pet_.careNow();
        float mystAvg = s.weight > 0 ? s.mystW / s.weight : s.stat[3];
        int lan = (int)(careAvg / 20.0f + 0.5f);
        char l1[40], l2[40], l3[40];
        std::snprintf(l1, sizeof(l1), "Care: %d of 5 lanterns", lan);
        std::snprintf(l2, sizeof(l2), "Stayed hidden: %s", mystAvg > 65 ? "YES" : mystAvg > 40 ? "MOSTLY" : "NO");
        std::snprintf(l3, sizeof(l3), "Rough nights: %d h", (int)s.neglectHours);
        if (revealForm_ == (uint8_t)Form::HOAX) {
          cv.textWrap(FLabel, 113, 390, 240, SPECIES_TEXT[s.species & 3].hoax, th::lantern, 2, 2, true);
        } else {
          cv.textCenter(FLabel, L::CX, 390, l1, th::text);
          cv.textCenter(FLabel, L::CX, 412, l2, th::text);
          cv.textCenter(FLabel, L::CX, 434, l3, th::text);
        }
      }
      if (t > 3000) cv.textCenter(FLabel, L::CX, adult ? 38 : 404, "Tap to continue", th::dim);
      break;
    }
    case Ov::HIKER: {
      drawPaperCard(83, 96, 300, 290);
      cv.textCenter(FTitle, L::CX, 150, "A hiker!", th::stamp);
      cv.textWrap(FBody, 103, 186, 260, "Someone with a camera is coming up the trail.", th::ink, 3, 0, true);
      int left = 10000 - (int)(ms_ - hikerShownMs_);
      if (left < 0) left = 0;
      cv.fillRect(113, 268, 240, 8, th::manila);
      cv.fillRect(113, 268, 240 * left / 10000, 8, th::ember);
      cv.fillRound(113, 294, 240, 64, 32, th::lantern);
      cv.roundRect(113, 294, 240, 64, 32, th::ink, 2);
      cv.textCenter(FHead, L::CX, 336, "HIDE!", th::ink);
      break;
    }
    case Ov::BURROW: {
      cv.fill(th::bg);
      cv.textCenter(FTitle, L::CX, 112, "Hibernating", th::moon);
      cv.textWrap(FBody, 93, 150, 280, "Left alone for days, it curled up in a burrow to wait for you.", th::text, 3, 0, true);
      cv.fillEllipse(L::CX, 310, 92, 34, gfx::hex(0x3A2E22));
      cv.fillEllipse(L::CX, 300, 72, 26, gfx::hex(0x4A3A2A));
      cv.text(FHand, 290, 268 - (int)((ms_ / 300) % 8) * 2, "z", th::moon);
      arc(cv, L::CX, 300, 100, 2, 0, 6.2832f, th::line);
      if (holdId_ == 3 && holdP_ > 0) drawRing(L::CX, 300, 100, holdP_, th::lantern);
      cv.textCenter(FBodyB, L::CX, 432, "HOLD TO DIG OUT", th::lantern);
      break;
    }
    case Ov::FAREWELL: {
      cv.fill(th::bg);
      for (int i = 0; i < 6; ++i) cv.fillEllipse(40 + i * 78, 300 + (i % 2) * 30, 90, 16, th::fog, 45);
      cv.textCenter(FTitle, L::CX, 118, "Case closed", th::moon);
      cv.textWrap(FBody, 83, 166, 300, farewell_, th::text, 4, 0, true);
      cv.textCenter(FLabel, L::CX, 418, "Tap for a new egg tray", th::dim);
      break;
    }
    case Ov::PANTS: {
      cv.textCenter(FTitle, L::CX, 100, "Tonight's pants", th::moon);
      for (int i = 0; i < 4; ++i) {
        int y = 120 + i * 64;
        cv.fillRound(93, y, 280, 56, 14, th::paper);
        cv.roundRect(93, y, 280, 56, 14, th::ink, 2);
        cv.sprite(*iconSprite(Icon::PANTS), 113, y + 14);
        cv.text(FBodyB, 155, y + 35, PANTS[i], th::ink);
      }
      cv.textCenter(FBodyB, L::CX, 414, "Never mind", th::fog);
      break;
    }
    case Ov::CLOCK: {
      drawPaperCard(83, 64, 300, 346);
      cv.textCenter(FHead, L::CX, 100, "Set the clock", th::ink);
      Civil c = civil((int64_t)now_ + P.utcOffset(now_));
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%u/%u  %d:%02d %s", c.mo, c.d, c.h % 12 == 0 ? 12 : c.h % 12, c.m, c.h < 12 ? "AM" : "PM");
      cv.textCenter(FBodyB, L::CX, 130, buf, th::ink);
      cv.textCenter(FLabel, L::CX, 152, P.clockVerified() ? "Clock set" : "Not set", th::sepia);
      static const char* lab[6] = {"+1 hour", "-1 hour", "+10 min", "-10 min", "+1 day", "-1 day"};
      for (int i = 0; i < 6; ++i) {
        int bx = 103 + (i % 2) * 136, by = 164 + (i / 2) * 52;
        cv.fillRound(bx, by, 124, 46, 10, th::manila);
        cv.roundRect(bx, by, 124, 46, 10, th::ink, 2);
        cv.textCenter(FLabel, bx + 62, by + 29, lab[i], th::ink);
      }
      drawButton(123, 330, 220, 50, "DONE", true);
      break;
    }
    case Ov::RESULT:
      drawPaperCard(93, 118, 280, 236);
      cv.textWrap(FBody, 113, 160, 240, farewell_, th::ink, 3, 6, true);
      cv.textCenter(FLabel, L::CX, 336, "Tap to continue", th::sepia);
      break;
    case Ov::CONFIRM_RESET:
      drawPaperCard(83, 76, 300, 330);
      cv.textCenter(FTitle, L::CX, 128, "Start over?", th::stamp);
      cv.textWrap(FBody, 103, 164, 260, "Your cryptid goes back to the woods with no souvenir. The evidence stays.", th::ink, 3, 0, true);
      drawHoldButton(113, 270, 240, 56, "HOLD TO RESET", 5);
      cv.textCenter(FBodyB, L::CX, 372, "Cancel", th::ink);
      break;
    default: break;
  }
}

// ---- minigames ------------------------------------------------------------------
void App::drawGame() {
  const State& s = pet_.state();
  const SpeciesText& T = SPECIES_TEXT[s.species & 3];
  uint32_t t = ms_ - g_.start;
  float ts = t / 1000.0f;
  int form = formIdx();
  if (form == 0) form = 1;
  cv.fill(th::bg);

  switch (g_.type) {
    case Game::WALTZ: {
      cv.vgradient(0, 0, gfx::W, gfx::H, gfx::hex(0x070A14), gfx::hex(0x18213A), 10);
      int gx = (int)(L::G_WX + L::G_WAX * std::sin(ts * 0.9f)), gy = (int)(L::G_WY + L::G_WAY * std::sin(ts * 0.6f + 1.0f));
      cv.fillCircle(gx, gy, L::G_RING + 14, th::lantern, 30);
      cv.fillCircle(gx, gy, L::G_RING, th::lantern, 40);
      cv.circle(gx, gy, L::G_RING, th::lantern);
      cv.fillCircle(gx, gy, L::G_CORE, gfx::hex(0xF5D08A));
      cv.circle(gx, gy, L::G_CORE, th::ember);
      cv.sprite(*creatureSprite(s.species, form, g_.dizzy > 0.5f ? 2 : 0), (int)g_.px - 100, (int)g_.py - 120);
      arc(cv, L::CX, L::CY, 210, 3, 4.0f, 4.0f + 1.4f * (g_.dizzy > 1 ? 1 : g_.dizzy), th::ember);
      cv.textCenter(FLabel, L::CX, 400, hasMotion_ ? "Stay in the glow. Drag or tilt." : "Stay in the glow.", th::fog);
      break;
    }
    case Game::FORAGE: {
      cv.vgradient(0, 0, gfx::W, gfx::H, gfx::hex(0x0D1424), gfx::hex(0x1C2A20), 8);
      for (int i = 0; i < 6; ++i) {
        int x = i % 2 == 0 ? L::G_FX0 : L::G_FX1, y = L::G_FY0 + (i / 2) * L::G_FDY;
        cv.fillEllipse(x, y + 24, 64, 38, gfx::hex(0x1C3A24));
        cv.fillEllipse(x - 18, y + 10, 32, 24, gfx::hex(0x245030));
        cv.fillEllipse(x + 20, y + 14, 32, 24, gfx::hex(0x2A5A36));
      }
      for (auto& o : g_.obj) {
        if (!o.alive) continue;
        if (o.kind == 0) { cv.fillCircle(o.x - 9, o.y, 13, gfx::hex(0x6B5BA8)); cv.fillCircle(o.x + 9, o.y + 4, 13, gfx::hex(0x5A4A98)); }
        else if (o.kind == 1) { cv.fillEllipse(o.x, o.y - 6, 22, 15, th::ember); cv.fillRect(o.x - 6, o.y, 12, 18, th::paper); cv.fillCircle(o.x - 8, o.y - 9, 4, th::paper); }
        else { cv.fillEllipse(o.x, o.y, 15, 22, gfx::hex(0x7A5A3A)); for (int k = -15; k < 18; k += 8) cv.fillRect(o.x - 11, o.y + k, 22, 2, gfx::hex(0x4A3422)); }
      }
      cv.textCenter(FLabel, L::CX, 400, "Snacks! No pinecones.", th::fog);
      break;
    }
    case Game::HOP: {
      cv.vgradient(0, 0, gfx::W, 250, gfx::hex(0x070A14), gfx::hex(0x18213A), 8);
      cv.fillRect(0, 250, gfx::W, gfx::H - 250, gfx::hex(0x1B3A40));
      for (int x = 0; x < gfx::W; x += 22) cv.fillRect(x + (int)(ts * 40) % 22, 252, 11, 2, th::loch);
      cv.sprite(*creatureSprite(s.species, form, 0), 10, 120, gfx::Tint(), 2);
      for (auto& o : g_.obj) {
        if (!o.alive) continue;
        if (o.kind) {
          cv.fillRect(o.x - 38, o.y + 4, 76, 14, gfx::hex(0x6B4A2A));
          cv.fillRect(o.x - 26, o.y - 10, 52, 14, gfx::hex(0x8A6A4A));
          cv.fillRect(o.x - 1, o.y - 40, 3, 30, th::paper);
        } else {
          cv.circle(o.x, o.y, 17, th::moon);
          cv.circle(o.x, o.y, 16, th::moon);
          cv.fillCircle(o.x - 6, o.y - 6, 4, th::moon);
        }
      }
      cv.textCenter(FLabel, L::CX, 400, "Bubbles. Not boats.", th::fog);
      break;
    }
    case Game::STROLL: {
      cv.vgradient(0, 0, gfx::W, 300, gfx::hex(0x070A14), gfx::hex(0x18213A), 8);
      cv.fillRect(0, 300, gfx::W, gfx::H - 300, gfx::hex(0x1D211F));
      for (int i = 0; i < 8; ++i) {
        int x = ((i * 80 - (g_.steps * 16)) % 640 + 640) % 640 - 80;
        cv.fillRect(x, 314, 48, 5, gfx::hex(0x3A403A));
      }
      int sway = (g_.steps % 2) ? 4 : -4;
      cv.sprite(*creatureSprite(s.species, form, g_.wobbles > 3 ? 2 : 0), L::PET_X + sway, 110);
      bool down = ptr_.down && ptr_.y > L::G_TOP && ptr_.y < L::G_BOT;
      cv.fillRectA(0, L::G_TOP, L::G_SPLIT, L::G_BOT - L::G_TOP, th::moon, down && ptr_.x < L::G_SPLIT ? 30 : 8);
      cv.fillRectA(L::G_SPLIT, L::G_TOP, gfx::W - L::G_SPLIT, L::G_BOT - L::G_TOP, th::moon, down && ptr_.x >= L::G_SPLIT ? 30 : 8);
      cv.textCenter(FBodyB, 120, 360, "LEFT", th::fog);
      cv.textCenter(FBodyB, 346, 360, "RIGHT", th::fog);
      if (g_.walk && hasMotion_) cv.textCenter(FLabel, L::CX, 396, "Or just walk. He counts.", th::fog);
      else cv.textCenter(FLabel, L::CX, 396, "Left, right. Calmly.", th::fog);
      break;
    }
  }

  // Header in the top of the circle, the timer around the rim, QUIT at the bottom.
  fade(cv, 0, 96, th::bg, 220, 0);
  cv.textCenter(FHead, L::CX, 50, T.game, th::moon);
  char sc[40];
  if (g_.type == Game::WALTZ) std::snprintf(sc, sizeof(sc), "Glow %d%%", g_.score);
  else if (g_.type == Game::STROLL) std::snprintf(sc, sizeof(sc), "Steps %d  Wobble %d", g_.steps, g_.wobbles);
  else std::snprintf(sc, sizeof(sc), "Got %d  Oops %d", g_.score, g_.misses);
  cv.textCenter(FLabel, L::CX, 76, sc, th::lantern);
  float left = 1.0f - (float)(t > GAME_MS ? GAME_MS : t) / (float)GAME_MS;
  arc(cv, L::CX, L::CY, 228, 3, 0, 6.2832f * left, th::lantern);
  cv.fillRound(L::G_QUIT_X, L::G_QUIT_Y, L::G_QUIT_W, 38, 19, th::panel);
  cv.textCenter(FLabel, L::CX, L::G_QUIT_Y + 26, "QUIT", th::dim);
}

// ---- hit testing (mirrors the drawing above) ---------------------------------------
Hit App::hitAt(int x, int y) const {
  if (scr_ == Scr::GAME) {
    if (in(x, y, L::G_QUIT_X, L::G_QUIT_Y, L::G_QUIT_W, L::G_QUIT_H)) return {HIT_QUIT, 0};
    return {HIT_NONE, 0};
  }
  if (ov_ != Ov::NONE) {
    switch (ov_) {
      case Ov::BURROW:   // the burrow and its label below
        if (inCircle(x, y, L::CX, 300, 110) || in(x, y, 123, 404, 220, 44)) return {HIT_HOLD_DIG, 0};
        break;
      case Ov::CONFIRM_RESET:
        if (in(x, y, 113, 270, 240, 56)) return {HIT_HOLD_RESET, 0};
        if (in(x, y, 143, 340, 180, 52)) return {HIT_CANCEL, 0};
        break;
      case Ov::HIKER: if (in(x, y, 103, 284, 260, 84)) return {HIT_HIDE, 0}; break;
      case Ov::PANTS:
        for (int i = 0; i < 4; ++i) if (in(x, y, 93, 120 + i * 64, 280, 56)) return {HIT_PANTS, (int8_t)i};
        if (in(x, y, 143, 384, 180, 52)) return {HIT_CANCEL, 0};
        break;
      case Ov::CLOCK:
        for (int i = 0; i < 6; ++i) {
          int bx = 103 + (i % 2) * 136, by = 164 + (i / 2) * 52;
          if (in(x, y, bx, by, 124, 46)) return {HIT_CLOCK, (int8_t)i};
        }
        if (in(x, y, 123, 330, 220, 50)) return {HIT_DONE, 0};
        break;
      default: break;
    }
    return {HIT_ANY, 0};
  }
  switch (scr_) {
    case Scr::BOOT: return {HIT_ANY, 0};
    case Scr::TRAY:
      if (inCircle(x, y, L::EGG_CX, L::EGG_CY, L::EGG_RING + 10)) return {HIT_HOLD_INCUBATE, 0};
      if (inCircle(x, y, L::ARROW_L, L::EGG_CY, L::ARROW_HIT)) return {HIT_PREV, 0};
      if (inCircle(x, y, L::ARROW_R, L::EGG_CY, L::ARROW_HIT)) return {HIT_NEXT, 0};
      return {HIT_NONE, 0};
    case Scr::NAME:
      if (in(x, y, L::NAME_BTN1_X, L::NAME_BTN_Y, L::NAME_BTN_W, L::NAME_BTN_H)) return {HIT_ANOTHER, 0};
      if (in(x, y, L::NAME_BTN2_X, L::NAME_BTN_Y, L::NAME_BTN_W, L::NAME_BTN_H)) return {HIT_USE, 0};
      return {HIT_NONE, 0};
    default: break;
  }
  if (scr_ != Scr::MAIN) return {HIT_NONE, 0};
  if (y >= L::TAB_HIT_Y0 && y < L::TAB_HIT_Y1) {
    for (int i = 0; i < 4; ++i)
      if (x >= tabX(i) - 2 && x < tabX(i) + L::TAB_W + 2) return {HIT_TAB, (int8_t)i};
  }
  if (y < L::CONTENT_Y) return {HIT_NONE, 0};
  switch (tab_) {
    case Tab::DEN:
      if (inCircle(x, y, L::PET_CX, L::PET_CY, L::PET_R)) return {HIT_PET, 0};
      if (in(x, y, L::BEST_X - 10, L::BEST_Y - 6, L::BEST_W + 20, L::BEST_H + 14)) return {HIT_BEST, 0};
      break;
    case Tab::CARE: {
      int ids[12];
      int n = careRows(ids, 12);
      for (int i = 0; i < n; ++i) {
        int ry = L::CONTENT_Y + 8 + i * (L::ROW_H + L::ROW_GAP) - scroll_[1];
        if (in(x, y, L::ROW_X, ry, L::ROW_W, L::ROW_H)) return {HIT_ROW, (int8_t)i};
      }
      break;
    }
    case Tab::FILE:
      if (pet_.state().form >= (uint8_t)Form::LEGEND) {
        int ry = contentH_[2] - 70 + L::CONTENT_Y - scroll_[2];
        if (in(x, y, L::COL_X, ry, L::COL_W, 56)) return {HIT_HOLD_RELEASE, 0};
      }
      break;
    case Tab::MORE: {
      int y0 = L::CONTENT_Y + L::ALMANAC_H - scroll_[3];
      for (int i = 0; i < 6; ++i) {
        int ry = y0 + i * (L::ROW_H + L::ROW_GAP);
        if (in(x, y, L::ROW_X, ry, L::ROW_W, L::ROW_H)) return {HIT_ROW, (int8_t)i};
      }
      break;
    }
  }
  return {HIT_NONE, 0};
}
#endif  // CRYPT_ROUND
