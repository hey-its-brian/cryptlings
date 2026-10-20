// Field Journal palette (Brief A2.1) and font handles.
#pragma once
#include "gfx.h"

namespace th {
using gfx::hex;
static const gfx::Color bg        = hex(0x0B0D12);
static const gfx::Color bg2       = hex(0x121722);
static const gfx::Color panel     = hex(0x181E2B);
static const gfx::Color line      = hex(0x2C3547);
static const gfx::Color text      = hex(0xE7E1D2);
static const gfx::Color dim       = hex(0x7A8396);
static const gfx::Color fog       = hex(0x8FA3B8);
static const gfx::Color moon      = hex(0xF2E6B8);
static const gfx::Color lantern   = hex(0xF5A642);
static const gfx::Color dusk      = hex(0x8E7CC3);
static const gfx::Color moss      = hex(0x7FB069);
static const gfx::Color ember     = hex(0xE5604D);
static const gfx::Color loch      = hex(0x4FA3A5);
static const gfx::Color manila    = hex(0xD9C79A);
static const gfx::Color manilaDk  = hex(0x8F7F5C);
static const gfx::Color paper     = hex(0xEFE6D0);
static const gfx::Color ink       = hex(0x2A2320);
static const gfx::Color sepia     = hex(0x5C4A3A);
static const gfx::Color stamp     = hex(0x9E3426);
static const gfx::Color stampBlue = hex(0x2E5A8C);
static const gfx::Color confirmed = hex(0x3F6B34);
static const gfx::Color pink      = hex(0xE58FA0);
static const gfx::Color firefly   = hex(0xE8F08A);
}  // namespace th

// Font roles. Baked per screen (assets/fonts.h or fonts_round.h, included only
// in fonts.cpp). Sizes are for the rectangle build / the round build.
extern const Font& FLabel;    // Courier Prime Bold 12 / 18: status, small labels, stamps
extern const Font& FBody;     // Courier Prime 14 / 20: body copy
extern const Font& FBodyB;    // Courier Prime Bold 14 / 20: buttons, row titles
extern const Font& FHead;     // Grenze SemiBold 16 / 24: headers, tabs, species names
extern const Font& FHand;     // Patrick Hand 16 / 26: polaroid captions, notes
extern const Font& FTitle;    // Grenze Gotisch 24 / 36: screen titles
extern const Font& FSplash;   // Grenze Gotisch 32 / 48: splash
