#pragma once
#include "gfx.h"

// mood index: 0 happy, 1 neutral, 2 sad, 3 sleep, 4 blink
// form index: 0 egg, 1 whisper, 2 rumor, 3 sighting, 4 legend, 5 blurry
const Sprite* creatureSprite(int species, int form, int mood);
// tod: 0 dusk, 1 night, 2 dawn, 3 day. 144 x 128 (round build: 466 x 466).
const Sprite* sceneSprite(int species, int tod);

enum class Icon { BELLY, CHEER, REST, MYST, TIDY, PLAY, LIGHTS, CASE, LOCH, WALK, PANTS, LAMP, FORAGE, TREAT };
const Sprite* iconSprite(Icon i);   // 16 x 16 (round build: 28 x 28)
