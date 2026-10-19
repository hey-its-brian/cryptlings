// App drawing: every screen and overlay. Geometry comes from layout.h so the
// hit tests in app.cpp line up with what is drawn here.
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
        int maxS = contentH_[i] - (gfx::H - L::CONTENT_Y);
        if (maxS < 0) maxS = 0;
        if (i && scroll_[i] > maxS) { scroll_[i] = maxS; dirty_ = true; }
      }
      cv.noClip();
      drawStatus();
      drawTabs();
      break;
  }
  cv.noClip();
  drawOverlay();
  drawToast();
  if (powerHold_ > 0) {
    cv.fillRectA(0, 0, gfx::W, gfx::H, th::bg, 220);
    drawPaperCard(14, 96, 144, 132);
    cv.textCenter(F16H, 86, 122, "Keep holding", th::ink);
    cv.textCenter(F16H, 86, 140, "to power off", th::ink);
    drawRing(86, 182, 24, powerHold_, th::lantern);
    cv.textCenter(F12B, 86, 220, "Your pet is saved.", th::sepia);
  }
}

void App::renderPowerOff() {
  save(true);
  cv.noClip();
  cv.fill(th::bg);
  for (int i = 0; i < 4; ++i) cv.fillEllipse(20 + i * 44, 276 + (i % 2) * 12, 50, 9, th::fog, 45);
  cv.moon(86, 92, 22, Pet::moonPhase(now_), th::moon, th::line);
  cv.textCenter(F24, 86, 160, "Powered off", th::moon);
  cv.textWrap(F14, 12, 190, 148, set_.hasPet ? "Your cryptid is curled up in its burrow. Press the button to wake it."
                                             : "Press the button to wake.", th::text, 2, 0, true);
}

// ---- chrome ----------------------------------------------------------------
void App::drawStatus() {
  cv.fillRect(0, 0, gfx::W, L::STATUS_H, th::panel);
  cv.moon(8, 8, 5, Pet::moonPhase(now_), th::moon, th::line);
  int64_t l = (int64_t)now_ + P.utcOffset(now_);
  int sec = (int)((l % 86400 + 86400) % 86400);
  int h = sec / 3600, m = (sec / 60) % 60;
  char buf[24];
  std::snprintf(buf, sizeof(buf), "%d:%02d%s", h % 12 == 0 ? 12 : h % 12, m, h < 12 ? "A" : "P");
  cv.text(F12B, 17, 12, buf, th::fog);
  int x = gfx::W - 4;
  int bat = P.batteryPercent();
  if (bat >= 0) {
    std::snprintf(buf, sizeof(buf), "%d%%", bat);
    x -= gfx::Canvas::textWidth(F12B, buf);
    cv.text(F12B, x, 12, buf, bat < 20 ? th::ember : th::fog);
    x -= 10;
    cv.rect(x, 3, 7, 10, th::fog);
    cv.fillRect(x + 2, 1, 3, 2, th::fog);
    int fill = bat * 6 / 100;
    cv.fillRect(x + 2, 4 + (6 - fill), 3, fill, bat < 20 ? th::ember : th::lantern);
    x -= 4;
  }
  if (!P.clockVerified()) {
    cv.fillCircle(x - 3, 8, 2, th::ember);   // clock not synced (see MORE)
  }
}

void App::drawTabs() {
  static const char* names[4] = {"Den", "Care", "File", "More"};
  cv.fillRect(0, L::TABS_Y, gfx::W, L::TABS_H, th::panel);
  for (int i = 0; i < 4; ++i) {
    int x = L::TAB_X0 + i * (L::TAB_W + L::TAB_GAP);
    bool on = (int)tab_ == i;
    cv.fillRound(x, L::TABS_Y + 2, L::TAB_W, L::TABS_H + 4, 5, on ? th::manila : th::manilaDk);
    cv.textCenter(F16H, x + L::TAB_W / 2, L::TABS_Y + 17, names[i], on ? th::ink : th::bg);
  }
  cv.hline(0, L::CONTENT_Y - 2, gfx::W, (int)tab_ == 0 ? th::bg : th::manila);
}

void App::drawPaperCard(int x, int y, int w, int h) {
  cv.fillRectA(x + 2, y + 3, w, h, th::bg, 140);
  cv.fillRound(x, y, w, h, 3, th::paper);
}

void App::drawButton(int x, int y, int w, int h, const char* label, bool primary, const void* icon) {
  if (primary) cv.fillRound(x - 2, y - 2, w + 4, h + 4, 7, th::lantern);
  cv.fillRound(x, y, w, h, 5, th::paper);
  cv.roundRect(x, y, w, h, 5, th::ink, 2);
  if (icon) {
    cv.sprite(*(const Sprite*)icon, x + w / 2 - 8, y + 7);
    cv.textCenter(F14B, x + w / 2, y + h - 9, label, th::ink);
  } else {
    cv.textCenter(F14B, x + w / 2, y + h / 2 + 5, label, th::ink);
  }
}

void App::drawHoldButton(int x, int y, int w, int h, const char* label, int id) {
  cv.fillRound(x, y, w, h, 6, th::paper);
  if (holdId_ == id && holdP_ > 0) {
    int fw = (int)(w * (holdP_ > 1 ? 1 : holdP_));
    cv.clip(x, y, fw, h);
    cv.fillRound(x, y, w, h, 6, th::lantern);
    cv.noClip();
  }
  cv.roundRect(x, y, w, h, 6, th::lantern, 2);
  cv.textCenter(F14B, x + w / 2, y + h / 2 + 5, label, th::ink);
}

void App::drawRing(int cx, int cy, int r, float p, Color c) {
  if (p <= 0) return;
  int steps = (int)(p * 120);
  for (int i = 0; i <= steps; ++i) {
    float a = -1.5708f + 6.2832f * i / 120.0f;
    int x = cx + (int)std::lround(r * std::cos(a)), y = cy + (int)std::lround(r * std::sin(a));
    cv.fillCircle(x, y, 2, c);
  }
}

void App::drawToast() {
  if (!toast_[0] || !toastUntil_) return;
  int lines = gfx::Canvas::wrapLines(F12B, 148, toast_);
  if (lines > 4) lines = 4;
  int h = lines * (F12B.line + 2) + 12;
  int y = 44;
  drawPaperCard(6, y, 160, h);
  cv.fillRectA(70, y - 4, 32, 9, th::manila, 190);   // tape
  cv.textWrap(F12B, 12, y + 6 + F12B.ascent, 148, toast_, th::ink, 2, 4);
}

// ---- creature + photo ---------------------------------------------------------
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
    for (int i = 0; i < 26; i += 3) cv.fillRect(x + 49, y + 40 + i, 2, 2, th::moon);
  }
}

void App::drawPhoto(int px, int py, bool live) {
  const State& s = pet_.state();
  int td = tod();
  cv.clip(px, py, L::PHOTO_W, L::PHOTO_H);
  cv.sprite(*sceneSprite(s.species, td), px, py);
  if (td != 3) cv.moon(px + 22, py + 18, 8, Pet::moonPhase(now_), td == 2 ? th::fog : th::moon, gfx::hex(0x1E2436));
  if (live && (td == 0 || td == 1)) {
    for (int i = 0; i < 4; ++i) {
      float t = ms_ / 1000.0f;
      int fx = px + 20 + i * 32 + (int)(std::sin(t * 0.7f + i * 2) * 8);
      int fy = py + 50 + (int)(std::cos(t * 0.9f + i) * 10);
      uint8_t a = (uint8_t)(120 + 110 * std::sin(t * 2 + i));
      cv.fillCircle(fx, fy, 2, th::firefly, a);
    }
  }
  int form = formIdx();
  bool hidden = s.hideReadyAt && now_ + (uint32_t)(pet_.tuning().hideCooldownMin * 60) - s.hideReadyAt < 20;
  int bob = live && moodIdx() != 3 ? ((ms_ / 420) % 2 ? -1 : 0) : 0;
  int cx = px + 22, cy = py + 22 + bob;
  if (s.hibernating) {
    cv.fillEllipse(px + 72, py + 112, 44, 16, gfx::hex(0x3A2E22));
    cv.fillEllipse(px + 72, py + 108, 36, 12, gfx::hex(0x4A3A2A));
    cv.text(F16P, px + 96, py + 92 - (int)((ms_ / 300) % 6), "z", th::moon);
  } else {
    drawCreature(cx, cy, form, moodIdx(), hidden ? 50 : 255);
    if (moodIdx() == 3 && !hidden) {
      int zz = (int)((ms_ / 250) % 12);
      cv.text(F16P, px + 96 + zz / 3, py + 46 - zz, "z", th::moon, (uint8_t)(255 - zz * 18));
    }
  }
  // Fog drifts in front.
  int drift = (int)((ms_ / 120) % 200);
  cv.fillEllipse(px - 30 + drift % 200, py + 114, 60, 7, th::fog, 60);
  cv.fillEllipse(px + 170 - (drift * 7 / 10) % 220, py + 120, 70, 6, th::fog, 45);
  if (holdId_ != 0 && holdP_ > 0 && scr_ == Scr::MAIN) drawRing(px + 72, py + 74, 34, holdP_, th::lantern);
  cv.noClip();
}

// ---- DEN ----------------------------------------------------------------------
void App::drawDen() {
  const State& s = pet_.state();
  // Polaroid
  cv.fillRectA(L::POL_X + 3, L::POL_Y + 4, L::POL_W, L::POL_H, gfx::hex(0x000000), 150);
  cv.fillRect(L::POL_X, L::POL_Y, L::POL_W, L::POL_H, th::paper);
  cv.fillRectA(L::POL_X - 4, L::POL_Y - 3, 26, 9, th::manila, 170);
  cv.fillRectA(L::POL_X + L::POL_W - 22, L::POL_Y - 3, 26, 9, th::manila, 170);
  drawPhoto(L::PHOTO_X, L::PHOTO_Y, true);
  // Stamp on the photo
  int form = (int)s.form;
  const char* sw = STAMP_WORD[form];
  int sw_w = gfx::Canvas::textWidth(F12B, sw) + 8;
  Color sc = form == 3 ? th::stamp : form == 4 ? th::confirmed : th::stampBlue;
  int sx = L::PHOTO_X + L::PHOTO_W - sw_w - 4, sy = L::PHOTO_Y + 4;
  cv.fillRectA(sx, sy, sw_w, 16, th::paper, 200);
  cv.rect(sx, sy, sw_w, 16, sc, 2);
  cv.text(F12B, sx + 4, sy + 12, sw, sc);
  // Caption
  int nw = cv.text(F16P, L::POL_X + 8, L::CAPTION_Y + 18, set_.name, th::ink);
  char st[24];
  std::snprintf(st, sizeof(st), "%s", form == (int)Form::BLURRY ? "BLURRY" : STAGE_NAME[form]);
  upper(st);
  int stw = gfx::Canvas::textWidth(F12B, st) + 8;
  int stx = L::POL_X + L::POL_W - stw - 6;
  if (L::POL_X + 8 + nw + 6 <= stx) {
    cv.rect(stx, L::CAPTION_Y + 5, stw, 16, th::stamp, 2);
    cv.text(F12B, stx + 4, L::CAPTION_Y + 17, st, th::stamp);
  }

  // Stats: icon + 5 pips each
  static const Icon icons[5] = {Icon::BELLY, Icon::CHEER, Icon::REST, Icon::MYST, Icon::TIDY};
  static const Color cols[5] = {th::lantern, th::pink, th::dusk, th::loch, th::moss};
  for (int i = 0; i < 5; ++i) {
    int cx = 6 + i * 32 + 16;
    float v = s.stat[i];
    bool egg = (Form)s.form == Form::EGG;
    gfx::Tint t;
    if (egg) t.a = 90;
    cv.sprite(*iconSprite(icons[i]), cx - 8, L::STATS_Y + 4, t);
    int on = egg ? 0 : (int)((v + 19.99f) / 20.0f);
    for (int k = 0; k < 5; ++k) {
      int x = cx - 14 + k * 6;
      Color c = k < on ? (v < 25 ? th::ember : cols[i]) : th::panel;
      cv.fillRect(x, L::STATS_Y + 26, 4, 8, c);
      if (k >= on) cv.rect(x, L::STATS_Y + 26, 4, 8, th::line);
    }
  }
  if ((Species)s.species == Species::NESSIE && (Form)s.form != Form::EGG) {
    int w = (int)(s.loch * 156 / 100);
    cv.fillRect(8, L::STATS_Y + 38, 156, 3, th::panel);
    cv.fillRect(8, L::STATS_Y + 38, w, 3, s.loch < 30 ? th::ember : th::loch);
  }
  // Hint
  char hb[48];
  cv.textCenter(F12B, gfx::W / 2, L::HINT_Y + 13, hintLine(hb, sizeof(hb)), th::lantern);
  // Buttons
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
  drawButton(L::BTN1_X, L::BTN_Y, L::BTN_W, L::BTN_H, lab, true, iconSprite(ic));
  drawButton(L::BTN2_X, L::BTN_Y, L::BTN_W, L::BTN_H, "CARE", false, iconSprite(Icon::CASE));
}

// ---- CARE ---------------------------------------------------------------------
void App::drawCare() {
  const State& s = pet_.state();
  const SpeciesText& T = SPECIES_TEXT[s.species & 3];
  cv.fillRect(0, L::CONTENT_Y - 2, gfx::W, gfx::H, th::bg2);
  cv.clip(0, L::CONTENT_Y, gfx::W, gfx::H - L::CONTENT_Y);
  int ids[12];
  int n = careRows(ids, 12);
  char sub[64];
  for (int i = 0; i < n; ++i) {
    int y = L::CONTENT_Y + 4 + i * (L::ROW_H + L::ROW_GAP) - scroll_[1];
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
        std::snprintf(sub, sizeof(sub), "%s", s.walked ? "Walked tonight" : "Evening Stroll");
        dim = s.walked; break;
      case A_PANTS: title = "PANTS"; ic = Icon::PANTS;
        std::snprintf(sub, sizeof(sub), "%s", s.pantsDone ? "Dressed for tonight" : "Pick tonight's pair");
        dim = s.pantsDone; break;
    }
    cv.fillRound(L::ROW_X, y, L::ROW_W, L::ROW_H, 5, dim ? th::manilaDk : th::paper);
    cv.roundRect(L::ROW_X, y, L::ROW_W, L::ROW_H, 5, th::ink, 2);
    cv.sprite(*iconSprite(ic), L::ROW_X + 8, y + 17);
    cv.text(F14B, L::ROW_X + 32, y + 21, title, th::ink);
    cv.textWrap(F12B, L::ROW_X + 32, y + 39, L::ROW_W - 38, sub, th::sepia, 0, 1);
  }
  contentH_[1] = n * (L::ROW_H + L::ROW_GAP) + 8;
}

// ---- FILE ---------------------------------------------------------------------
void App::drawFile() {
  const State& s = pet_.state();
  const SpeciesText& T = SPECIES_TEXT[s.species & 3];
  cv.fillRect(0, L::CONTENT_Y - 2, gfx::W, gfx::H, th::manila);
  cv.clip(0, L::CONTENT_Y, gfx::W, gfx::H - L::CONTENT_Y);
  int x = 8, w = 156;
  int y = L::CONTENT_Y + 6 - scroll_[2];
  y += F16H.ascent;
  cv.text(F16H, x, y, T.name, th::ink);
  y += 6;
  y = cv.textWrap(F12B, x, y + F12B.ascent, w, T.origin, th::sepia, 1);
  y += 2;
  y = cv.textWrap(F14, x, y + F14.ascent, w, T.bio, th::ink, 1);
  auto label = [&](const char* t) {
    y += 4;
    for (int i = 0; i < w; i += 4) cv.fillRect(x + i, y, 2, 1, th::sepia);
    y += 4 + F12B.ascent;
    cv.text(F12B, x, y, t, th::sepia);
    y += 6;
  };
  label("KNOWN QUIRK");
  y = cv.textWrap(F14, x, y + F14.ascent, w, T.quirk, th::ink, 1);
  label("PHOTO RECORD");
  static const int forms[5] = {0, 1, 2, 3, 4};
  for (int i = 0; i < 5; ++i) {
    int f = forms[i];
    if (i == 4) f = s.form >= (uint8_t)Form::LEGEND ? s.form : 4;
    bool got = s.stagePhotos & (1u << f);
    int tx = x + (i % 2) * 80, ty = y + (i / 2) * 82;
    cv.fillRect(tx, ty, 74, 78, th::paper);
    cv.fillRect(tx + 4, ty + 4, 66, 56, got ? th::bg2 : gfx::hex(0x3C3528));
    if (got) {
      cv.clip(tx + 4, ty + 4, 66, 56);
      int fi = f == (int)Form::HOAX ? 4 : f;
      gfx::Tint t;
      cv.sprite(*creatureSprite(s.species, fi, 1), tx + 12, ty + 6, t, 2);
      cv.clip(0, L::CONTENT_Y, gfx::W, gfx::H - L::CONTENT_Y);
    }
    char nm[16];
    std::snprintf(nm, sizeof(nm), "%s", got ? STAGE_NAME[f] : "???");
    if (f == (int)Form::BLURRY && got) std::snprintf(nm, sizeof(nm), "BLURRY");
    upper(nm);
    cv.textCenter(F12B, tx + 37, ty + 73, nm, th::ink);
  }
  y += 3 * 82 + 2;
  int found = 0;
  for (int i = 0; i < 16; ++i) if (s.evidence[s.species][i]) ++found;
  char lb[32];
  std::snprintf(lb, sizeof(lb), "EVIDENCE %d/16", found);
  label(lb);
  if (!found) {
    y = cv.textWrap(F14, x, y + F14.ascent, w, "Nothing yet. Check in at night, play, and keep up good care.", th::ink, 1);
  }
  for (int i = 0; i < 16; ++i) {
    uint8_t c = s.evidence[s.species][i];
    if (!c) continue;
    char line[80];
    std::snprintf(line, sizeof(line), "%s", T.evidence[i]);
    y = cv.textWrap(F14, x, y + F14.ascent, w, line, th::ink, 0);
    if (c >= 5) std::snprintf(line, sizeof(line), "CONFIRMED x%d", c);
    else if (c > 1) std::snprintf(line, sizeof(line), "%s x%d", TIER_NAME[(int)Pet::evidenceTier(i)], c);
    else std::snprintf(line, sizeof(line), "%s", TIER_NAME[(int)Pet::evidenceTier(i)]);
    cv.text(F12B, x + 6, y + F12B.ascent - 2, line, th::stamp);
    y += F12B.line + 4;
  }
  if (s.form >= (uint8_t)Form::LEGEND) {
    label("WHEN IT IS READY");
    y = cv.textWrap(F14, x, y + F14.ascent, w,
                    s.restless ? "Something out there is calling. You can let it go, and it will leave you a souvenir."
                               : "Adults can be released into the fog. Nothing is lost: the photos stay here.",
                    th::ink, 1);
    int by = y + 6;
    drawHoldButton(6, by, 160, 50, "HOLD TO RELEASE", 4);
    y = by + 60;
  }
  y += 8;
  contentH_[2] = y + scroll_[2] - L::CONTENT_Y;
  if (s.form >= (uint8_t)Form::LEGEND) contentH_[2] = (y - 8) + scroll_[2] - L::CONTENT_Y;
}

// ---- MORE (almanac + settings) -----------------------------------------------
void App::drawMore() {
  const State& s = pet_.state();
  cv.fillRect(0, L::CONTENT_Y - 2, gfx::W, gfx::H, th::bg2);
  cv.clip(0, L::CONTENT_Y, gfx::W, gfx::H - L::CONTENT_Y);
  int y = L::CONTENT_Y + 6 - scroll_[3];
  float p = Pet::moonPhase(now_);
  cv.text(F16H, 8, y + F16H.ascent, "Almanac", th::moon);
  cv.moon(26, y + 48, 17, p, th::moon, th::line);
  int tx = 50;
  cv.text(F12B, tx, y + 34, phaseName(p), th::text);
  float toFull = p <= 0.5f ? (0.5f - p) : (1.5f - p);
  int nights = (int)(toFull * 29.53f + 0.5f);
  char buf[48];
  if (Pet::nearFullMoon(now_, 18)) std::snprintf(buf, sizeof(buf), "Full tonight!");
  else std::snprintf(buf, sizeof(buf), "Full in %d nights", nights);
  cv.text(F12B, tx, y + 50, buf, th::lantern);
  cv.text(F12B, tx, y + 66, "(grows faster)", th::fog);
  cv.text(F12B, 8, y + 88, "Wakes 6 pm", th::text);
  cv.text(F12B, 8, y + 102, "Sleeps 7 am", th::text);
  cv.text(F12B, 8, y + 116, "Evolves 8 to 11 pm", th::text);
  cv.text(F12B, 8, y + 130, "Quiet 11 pm to 7 am", th::text);
  cv.text(F12B, 8, y + 144, P.clockVerified() ? "Clock synced" : "Clock: set it below", P.clockVerified() ? th::moss : th::ember);
  (void)s;
  static const char* sounds[3] = {"On", "Chimes only", "Off"};
  static const char* bright[3] = {"Low", "Medium", "High"};
  static const char* tos[3] = {"30 seconds", "1 minute", "2 minutes"};
  const char* titles[6] = {"SOUND", "BRIGHTNESS", "SCREEN OFF AFTER", "BATTERY SAVER", "SET CLOCK", "START OVER"};
  const char* subs[6] = {sounds[set_.sound % 3], bright[set_.brightness % 3], tos[set_.timeout % 3],
                         set_.saver ? "On: deep rest" : "Off", "Adjust date and time", "New egg tray"};
  int y0 = L::CONTENT_Y + 160 - scroll_[3];
  for (int i = 0; i < 6; ++i) {
    int ry = y0 + i * (L::ROW_H + L::ROW_GAP);
    cv.fillRound(L::ROW_X, ry, L::ROW_W, L::ROW_H, 5, th::panel);
    cv.roundRect(L::ROW_X, ry, L::ROW_W, L::ROW_H, 5, th::line, 2);
    cv.text(F14B, L::ROW_X + 10, ry + 21, titles[i], th::text);
    cv.text(F12B, L::ROW_X + 10, ry + 39, subs[i], i == 5 ? th::ember : th::lantern);
  }
  int yend = y0 + 6 * (L::ROW_H + L::ROW_GAP) + 6;
  yend = cv.textWrap(F12B, 8, yend + F12B.ascent, 156,
                     "CRYPTLINGS. A field journal for creatures nobody believes in. Be kind to them.", th::dim, 1);
  contentH_[3] = yend + 8 + scroll_[3] - L::CONTENT_Y;
}

// ---- BOOT ---------------------------------------------------------------------
void App::drawBoot() {
  cv.fill(th::bg);
  cv.fillRound(6, 22, 160, 276, 4, th::manila);
  cv.fillRound(14, 14, 64, 16, 4, th::manila);   // folder tab
  int64_t l = (int64_t)now_ + P.utcOffset(now_);
  int sec = (int)((l % 86400 + 86400) % 86400);
  int h = sec / 3600, m = (sec / 60) % 60;
  static const char* mon[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  // civil date from days since epoch
  int64_t z = l / 86400 + 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  unsigned doe = (unsigned)(z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  unsigned d = doy - (153 * mp + 2) / 5 + 1;
  unsigned mo = mp < 10 ? mp + 3 : mp - 9;
  char subj[24], stat[24], filed[24], moonl[28], caseNo[24];
  bool hasPet = set_.hasPet;
  char sp[16];
  std::snprintf(sp, sizeof(sp), "%s", hasPet ? SPECIES_TEXT[pet_.state().species & 3].shortName : "UNKNOWN");
  upper(sp);
  std::snprintf(subj, sizeof(subj), "SUBJ: %s", sp);
  std::snprintf(stat, sizeof(stat), "STAT: %s", hasPet ? STAMP_WORD[pet_.state().form % 7] : "UNVERIFIED");
  std::snprintf(filed, sizeof(filed), "FILED: %s %u %d:%02d%s", mon[(mo + 11) % 12], d, h % 12 == 0 ? 12 : h % 12, m, h < 12 ? "A" : "P");
  char ph[20];
  std::snprintf(ph, sizeof(ph), "%s", phaseName(Pet::moonPhase(now_)));
  upper(ph);
  std::snprintf(moonl, sizeof(moonl), "MOON: %s", ph);
  std::snprintf(caseNo, sizeof(caseNo), "CASE FILE #%04u", set_.caseNo);
  const char* lines[] = {"DEPT. OF ODD FAUNA", "FIELD OFFICE 7", "", caseNo, subj, stat, filed, moonl, "",
                         "NOTES: Something is", "in the woods. It is", "probably fine."};
  int n = sizeof(lines) / sizeof(lines[0]);
  int chars = (int)((ms_ - bootStart_) / 20);
  int y = 46;
  for (int i = 0; i < n && chars > 0; ++i) {
    char buf[32];
    int len = (int)std::strlen(lines[i]);
    int k = chars < len ? chars : len;
    std::memcpy(buf, lines[i], k);
    buf[k] = 0;
    cv.text(F12B, 14, y, buf, th::ink);
    chars -= len + 4;
    y += 17;
  }
  if (chars > 0) {
    cv.rect(40, 248, 92, 22, th::stampBlue, 2);
    cv.textCenter(F12B, 86, 263, hasPet ? "CASE OPEN" : "UNVERIFIED", th::stampBlue);
    if ((ms_ / 600) % 2) cv.textCenter(F16P, 86, 290, "Tap to open the file", th::sepia);
  }
}

// ---- TRAY + NAME ----------------------------------------------------------------
void App::drawTray() {
  cv.fill(th::bg);
  drawStatus();
  const SpeciesText& T = SPECIES_TEXT[trayPick_];
  char k[24];
  std::snprintf(k, sizeof(k), "CASE FILE #%04u", set_.caseNo);
  cv.textCenter(F12B, 86, 32, k, th::fog);
  cv.textCenter(F24, 86, 56, "Specimen tray", th::moon);
  // nest glow
  cv.fillEllipse(86, L::EGG_Y + 96, 46, 9, th::lantern, 40);
  int bob = (ms_ / 500) % 2 ? -1 : 0;
  cv.sprite(*creatureSprite(trayPick_, 0, (ms_ / 3000) % 3 == 0 ? 0 : 1), 36, L::EGG_Y + bob);
  cv.textCenter(F24, 22, L::ARROW_Y + 28, "<", th::lantern);
  cv.textCenter(F24, 150, L::ARROW_Y + 28, ">", th::lantern);
  for (int i = 0; i < 4; ++i) cv.fillCircle(68 + i * 12, 176, 2, i == trayPick_ ? th::lantern : th::line);
  cv.textCenter(F16H, 86, 196, T.name, th::text);
  if (pet_.state().raisedMask & (1u << trayPick_)) {
    cv.fillRectA(110, 100, 56, 16, th::manila, 220);
    cv.text(F12B, 113, 112, "SEEN", th::stamp);
  }
  cv.clip(0, L::TRAY_BIO_Y, gfx::W, L::TRAY_BIO_H);
  int y = L::TRAY_BIO_Y - trayScroll_;
  y = cv.textWrap(F16P, 8, y + F16P.ascent, 156, T.eggHint, th::lantern, 0);
  y = cv.textWrap(F12B, 8, y + F12B.ascent, 156, T.quirk, th::fog, 1);
  y = cv.textWrap(F12B, 8, y + F12B.ascent + 2, 156, T.bio, th::text, 1);
  cv.noClip();
  cv.fillRectA(0, L::TRAY_BIO_Y + L::TRAY_BIO_H - 6, gfx::W, 6, th::bg, 160);
  drawHoldButton(L::HOLD_X, L::HOLD_Y, L::HOLD_W, L::HOLD_H, "HOLD TO INCUBATE", 1);
}

void App::drawName() {
  cv.fill(th::bg);
  drawStatus();
  const SpeciesText& T = SPECIES_TEXT[pet_.state().species & 3];
  cv.textCenter(F12B, 86, 34, "NAME THE SPECIMEN", th::fog);
  cv.fillEllipse(86, 158, 46, 9, th::lantern, 40);
  cv.sprite(*creatureSprite(pet_.state().species, 0, 0), 36, 60 + ((ms_ / 500) % 2 ? -1 : 0));
  cv.textCenter(F24, 86, 200, T.names[nameIdx_ % 6], th::moon);
  cv.textCenter(F12B, 86, 222, "Written on the egg", th::dim);
  cv.textCenter(F12B, 86, 238, "in pencil. Lightly.", th::dim);
  drawButton(L::BTN1_X, L::BTN_Y, L::BTN_W, L::BTN_H, "ANOTHER", false);
  drawButton(L::BTN2_X, L::BTN_Y, L::BTN_W, L::BTN_H, "USE IT", true);
}

// ---- overlays -------------------------------------------------------------------
void App::drawOverlay() {
  if (ov_ == Ov::NONE) return;
  const State& s = pet_.state();
  cv.fillRectA(0, 0, gfx::W, gfx::H, th::bg, 210);
  switch (ov_) {
    case Ov::REVEAL: {
      uint32_t t = ms_ - revealStart_;
      int px = 16, py = 36;
      cv.fillRect(px + 3, py + 4, 140, 172, gfx::hex(0x000000));
      cv.fillRect(px, py, 140, 172, th::paper);
      cv.vgradient(px + 6, py + 6, 128, 128, gfx::hex(0x0D1424), gfx::hex(0x1D2A22), 5);
      cv.clip(px + 6, py + 6, 128, 128);
      int f = revealForm_ == (uint8_t)Form::HOAX ? 4 : revealForm_;
      gfx::Tint tn;
      int mood = 0;
      if (f == 5) mood = 1;
      cv.sprite(*creatureSprite(s.species, f, mood), px + 20, py + 22, tn);
      // develop: paper fades out over 2.4 s in 8 steps
      int step = (int)(t / 300);
      if (step < 8) cv.fillRectA(px + 6, py + 6, 128, 128, th::paper, (uint8_t)(255 - step * 32));
      cv.noClip();
      if (t > 2600) {
        char w[24];
        std::snprintf(w, sizeof(w), "%s", STAGE_NAME[revealForm_]);
        upper(w);
        // Blackletter capitals are hard to read, so stamps use Grenze (Brief A8 Q6).
        int ww = gfx::Canvas::textWidth(F16H, w) + 20;
        if (ww > 136) ww = 136;
        int sx = 86 - ww / 2, sy = py + 140;
        cv.rect(sx, sy, ww, 26, th::stamp, 3);
        cv.textCenter(F16H, 86, sy + 19, w, th::stamp);
      }
      const char* cap = "";
      char cb[64];
      if (revealForm_ == (uint8_t)Form::LEGEND) cap = "A true Legend.";
      else if (revealForm_ == (uint8_t)Form::BLURRY) cap = "Still yours. Still out there.";
      else if (revealForm_ == (uint8_t)Form::HOAX) cap = "Debunked. Lovingly.";
      else { std::snprintf(cb, sizeof(cb), "Now a %s.", STAGE_NAME[revealForm_]); cap = cb; }
      if (t <= 2600) cv.textCenter(F16P, 86, py + 160, "Developing...", th::sepia);
      cv.textCenter(F16P, 86, py + 194, cap, th::moon);
      if (revealForm_ >= (uint8_t)Form::LEGEND && t > 2600) {
        float careAvg = s.weight > 0 ? s.careW / s.weight : pet_.careNow();
        float mystAvg = s.weight > 0 ? s.mystW / s.weight : s.stat[3];
        int lan = (int)(careAvg / 20.0f + 0.5f);
        char l1[40], l2[40], l3[40];
        std::snprintf(l1, sizeof(l1), "Care: %d of 5 lanterns", lan);
        std::snprintf(l2, sizeof(l2), "Stayed hidden: %s", mystAvg > 65 ? "YES" : mystAvg > 40 ? "MOSTLY" : "NO");
        std::snprintf(l3, sizeof(l3), "Rough nights: %d h", (int)s.neglectHours);
        cv.text(F12B, 12, 246, l1, th::text);
        cv.text(F12B, 12, 262, l2, th::text);
        cv.text(F12B, 12, 278, l3, th::text);
        if (revealForm_ == (uint8_t)Form::HOAX)
          cv.textWrap(F12B, 12, 294, 150, SPECIES_TEXT[s.species & 3].hoax, th::lantern, 1, 1);
      }
      if (t > 3000) cv.textCenter(F12B, 86, 312, "Tap to continue", th::dim);
      break;
    }
    case Ov::HIKER: {
      drawPaperCard(10, 60, 152, 200);
      cv.textCenter(F24, 86, 92, "A hiker!", th::stamp);
      cv.textWrap(F14, 18, 116, 138, "Someone with a camera is coming up the trail.", th::ink, 1, 0, true);
      int left = 10000 - (int)(ms_ - hikerShownMs_);
      if (left < 0) left = 0;
      cv.fillRect(18, 180, 136, 6, th::manila);
      cv.fillRect(18, 180, 136 * left / 10000, 6, th::ember);
      cv.fillRound(16, 196, 140, 52, 6, th::lantern);
      cv.roundRect(16, 196, 140, 52, 6, th::ink, 2);
      cv.textCenter(F16H, 86, 228, "HIDE!", th::ink);
      break;
    }
    case Ov::BURROW: {
      cv.fill(th::bg);
      cv.textCenter(F24, 86, 50, "Hibernating", th::moon);
      cv.textWrap(F14, 12, 76, 148, "Left alone for days, it curled up in a burrow to wait for you.", th::text, 1, 0, true);
      cv.fillEllipse(86, 186, 56, 22, gfx::hex(0x3A2E22));
      cv.fillEllipse(86, 180, 44, 16, gfx::hex(0x4A3A2A));
      cv.text(F16P, 116, 160 - (int)((ms_ / 300) % 8), "z", th::moon);
      drawHoldButton(16, 226, 140, 52, "HOLD TO DIG OUT", 3);
      break;
    }
    case Ov::FAREWELL: {
      cv.fill(th::bg);
      for (int i = 0; i < 5; ++i)
        cv.fillEllipse(30 + i * 30, 150 + (i % 2) * 20, 50, 10, th::fog, 50);
      cv.textCenter(F24, 86, 60, "Case closed", th::moon);
      cv.textWrap(F14, 12, 100, 148, farewell_, th::text, 2, 0, true);
      cv.textCenter(F12B, 86, 300, "Tap for a new egg tray", th::dim);
      break;
    }
    case Ov::PANTS: {
      cv.textCenter(F24, 86, 66, "Tonight's pants", th::moon);
      for (int i = 0; i < 4; ++i) {
        int y = 84 + i * 52;
        cv.fillRound(16, y, 140, 46, 5, th::paper);
        cv.roundRect(16, y, 140, 46, 5, th::ink, 2);
        cv.sprite(*iconSprite(Icon::PANTS), 26, y + 15);
        cv.text(F14B, 50, y + 28, PANTS[i], th::ink);
      }
      cv.textCenter(F12B, 86, 312, "Never mind", th::fog);
      break;
    }
    case Ov::CLOCK: {
      drawPaperCard(6, 44, 160, 272);
      cv.textCenter(F16H, 86, 66, "Set the clock", th::ink);
      int64_t l = (int64_t)now_ + P.utcOffset(now_);
      int sec = (int)((l % 86400 + 86400) % 86400);
      int h = sec / 3600, m = (sec / 60) % 60;
      int64_t z = l / 86400 + 719468;
      int64_t era = (z >= 0 ? z : z - 146096) / 146097;
      unsigned doe = (unsigned)(z - era * 146097);
      unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
      unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
      unsigned mp = (5 * doy + 2) / 153;
      unsigned d = doy - (153 * mp + 2) / 5 + 1;
      unsigned mo = mp < 10 ? mp + 3 : mp - 9;
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%u/%u  %d:%02d %s", mo, d, h % 12 == 0 ? 12 : h % 12, m, h < 12 ? "AM" : "PM");
      cv.textCenter(F14B, 86, 92, buf, th::ink);
      cv.textCenter(F12B, 86, 110, P.clockVerified() ? "Synced online" : "Not synced", th::sepia);
      static const char* lab[6] = {"+1 hour", "-1 hour", "+10 min", "-10 min", "+1 day", "-1 day"};
      for (int i = 0; i < 6; ++i) {
        int bx = 12 + (i % 2) * 76, by = 120 + (i / 2) * 50;
        cv.fillRound(bx, by, 72, 44, 5, th::manila);
        cv.roundRect(bx, by, 72, 44, 5, th::ink, 2);
        cv.textCenter(F12B, bx + 36, by + 27, lab[i], th::ink);
      }
      drawButton(12, 274, 148, 40, "DONE", true);
      break;
    }
    case Ov::RESULT:
      drawPaperCard(10, 90, 152, 140);
      cv.textWrap(F14, 18, 114, 136, farewell_, th::ink, 2, 0, true);
      cv.textCenter(F12B, 86, 220, "Tap to continue", th::sepia);
      break;
    case Ov::CONFIRM_RESET:
      drawPaperCard(10, 70, 152, 240);
      cv.textCenter(F24, 86, 102, "Start over?", th::stamp);
      cv.textWrap(F14, 18, 128, 136, "Your cryptid goes back to the woods with no souvenir. The evidence stays.", th::ink, 1, 0, true);
      drawHoldButton(16, 200, 140, 52, "HOLD TO RESET", 5);
      cv.textCenter(F14B, 86, 288, "Cancel", th::ink);
      break;
    default: break;
  }
}
