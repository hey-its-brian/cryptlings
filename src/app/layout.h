// Screen geometry shared by drawing and hit-testing (172 x 320 portrait).
// Follows Brief B11; every primary target is at least 44 px tall.
#pragma once

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
}  // namespace L
