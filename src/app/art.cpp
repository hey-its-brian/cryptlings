// Accessors for the baked art (assets/art.h is included only here).
#include "art.h"
#include "../assets/art.h"

const Sprite* creatureSprite(int species, int form, int mood) {
  if (species < 0 || species > 3) species = 0;
  if (form < 0 || form > 5) form = 4;
  if (mood < 0 || mood > 4) mood = 1;
  return CREATURE[species][form][mood];
}
const Sprite* sceneSprite(int species, int tod) { return SCENE[species & 3][tod & 3]; }

const Sprite* iconSprite(Icon i) {
  static const Sprite* const T[] = {&icon_belly, &icon_cheer, &icon_rest, &icon_myst, &icon_tidy,
                                    &icon_play, &icon_lights, &icon_case, &icon_loch, &icon_walk,
                                    &icon_pants, &icon_lamp, &icon_forage, &icon_treat};
  return T[(int)i];
}
