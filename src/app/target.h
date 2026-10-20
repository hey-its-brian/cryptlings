// Which screen the app is built for. One codebase, two layouts:
//   CRYPT_ROUND 0: Waveshare ESP32-C6-Touch-LCD-1.47, 172 x 320 portrait rectangle
//   CRYPT_ROUND 1: M5Stack StopWatch, 466 x 466 round AMOLED
// The engine, the app flow and the minigame rules are shared; drawing, hit
// testing and the baked art and fonts are per screen.
#pragma once

#ifndef CRYPT_ROUND
#define CRYPT_ROUND 0
#endif
