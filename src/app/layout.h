// Screen geometry shared by drawing, hit testing and the minigame rules.
// Rectangle build: 172 x 320 portrait, follows Brief B11; every primary
// target is at least 44 px tall. Round build: see the second half.
#pragma once
#include "target.h"

#if !CRYPT_ROUND
namespace L {
static const int STATUS_H = 16;
static const int TABS_Y = 16, TABS_H = 22, TAB_X0 = 4, TAB_W = 40, TAB_GAP = 1;
static const int TAB_HIT_H = 44;                 // tabs take touches from y 0 to 44
static const int CONTENT_Y = 40;

// DEN
static const int POL_X = 8, POL_Y = 42, POL_W = 156, POL_H = 158;
static const int PHOTO_X = 14, PHOTO_Y = 48, PHOTO_W = 144, PHOTO_H = 128;
static const int CAPTION_Y = 176;                // caption strip to POL_Y + POL_H
static const int STATS_Y = 202, STATS_H = 42;
static const int HINT_Y = 246, HINT_H = 18;
static const int BTN_Y = 266, BTN_H = 50, BTN_W = 80, BTN1_X = 4, BTN2_X = 88;

// lists (CARE, MORE)
static const int ROW_X = 6, ROW_W = 160, ROW_H = 50, ROW_GAP = 4;

// TRAY
static const int EGG_Y = 66;
static const int ARROW_Y = 92, ARROW_W = 44;
static const int TRAY_BIO_Y = 202, TRAY_BIO_H = 58;
static const int HOLD_Y = 264, HOLD_H = 52, HOLD_X = 8, HOLD_W = 156;

// scrolling tabs (CARE, FILE, MORE): the visible window
static const int VIEW_Y = CONTENT_Y, VIEW_H = 320 - CONTENT_Y;

// minigames (rules use these too)
static const int G_TOP = 40, G_BOT = 320;                      // play area below the header
static const int G_PX0 = 86, G_PY0 = 200;                      // where the player starts
static const int G_WX = 86, G_WY = 180, G_WAX = 46, G_WAY = 70; // Waltz glow path
static const int G_RING = 36, G_CORE = 12;                     // glow ring, hot bulb
static const int G_FX0 = 46, G_FX1 = 126, G_FY0 = 96, G_FDY = 72, G_FHIT = 30;   // Forage bushes
static const int G_HSPAWN = 190, G_HGONE = -30;                // Loch Hop drift, right to left
static constexpr float G_HSPD_BUB = 0.06f, G_HSPD_BOAT = 0.045f;   // px per ms
static const int G_HBOAT_Y = 178, G_HBUB_Y = 200, G_HBUB_SPAN = 90, G_HHIT_BOAT = 26, G_HHIT_BUB = 22;
static const int G_SPLIT = 86;                                 // Evening Stroll: left | right
}  // namespace L
#endif

#if CRYPT_ROUND
// Round build: 466 x 466 circle centred on (233, 233). Anything that must be
// read or touched stays inside the circle; the half-width of the circle at a
// height y is sqrt(233^2 - (y - 233)^2), which is what these numbers respect.
namespace L {
static const int CX = 233, CY = 233, R = 233;
static const int STATUS_BASE = 34;                  // time + battery baseline (top of the circle)
static const int TABS_Y = 46, TABS_H = 32, TAB_W = 70, TAB_STEP = 74;   // four pills, centred
static const int TAB_HIT_Y0 = 38, TAB_HIT_Y1 = 92;
static const int CONTENT_Y = 86;                    // below the tabs

// DEN: the scene fills the circle, the creature stands on the horizon
static const int PET_X = 133, PET_Y = 92;           // 200 px creature box
static const int PET_CX = 233, PET_CY = 190, PET_R = 100;   // touch + cuddle ring
static const int NAME_BASE = 316;
static const int STATS_Y = 324, STATS_X0 = 83, STATS_STEP = 60;
static const int HINT_BASE = 392;
static const int BEST_X = 143, BEST_Y = 402, BEST_W = 180, BEST_H = 46;

// lists (CARE, MORE) and the FILE column
static const int ROW_X = 73, ROW_W = 320, ROW_H = 60, ROW_GAP = 8;
static const int COL_X = 83, COL_W = 300;
static const int ALMANAC_H = 236;                   // MORE: almanac block above the settings rows
static const int VIEW_Y = CONTENT_Y, VIEW_H = 314;  // scroll window (the circle narrows below ~400)

// TRAY + NAME
static const int EGG_CX = 233, EGG_CY = 214, EGG_RING = 92;
static const int ARROW_L = 56, ARROW_R = 410, ARROW_HIT = 44;
static const int NAME_BTN_Y = 384, NAME_BTN_H = 48, NAME_BTN_W = 114, NAME_BTN1_X = 115, NAME_BTN2_X = 237;

// minigames
static const int G_TOP = 84, G_BOT = 412;          // play area between the header and QUIT
static const int G_QUIT_X = 173, G_QUIT_Y = 414, G_QUIT_W = 120, G_QUIT_H = 48;
static const int G_PX0 = 233, G_PY0 = 330;
static const int G_WX = 233, G_WY = 250, G_WAX = 110, G_WAY = 100;
static const int G_RING = 70, G_CORE = 24;
static const int G_FX0 = 160, G_FX1 = 306, G_FY0 = 150, G_FDY = 92, G_FHIT = 52;
static const int G_HSPAWN = 480, G_HGONE = -60;
static constexpr float G_HSPD_BUB = 0.12f, G_HSPD_BOAT = 0.09f;
static const int G_HBOAT_Y = 238, G_HBUB_Y = 284, G_HBUB_SPAN = 104, G_HHIT_BOAT = 50, G_HHIT_BUB = 40;
static const int G_SPLIT = 233;
}  // namespace L
#endif
