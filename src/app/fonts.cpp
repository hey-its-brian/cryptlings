// The one translation unit that owns the baked font data.
#include "theme.h"
#if CRYPT_ROUND
#include "../assets/fonts_round.h"
#else
#include "../assets/fonts.h"
#endif

const Font& FLabel  = font_body12b;
const Font& FBody   = font_body14;
const Font& FBodyB  = font_body14b;
const Font& FHead   = font_head16;
const Font& FHand   = font_hand16;
const Font& FTitle  = font_title24;
const Font& FSplash = font_title32;
