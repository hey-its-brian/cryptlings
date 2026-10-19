// Species copy: names, bios, quirks, care flavor, evidence, name generator.
#pragma once

struct SpeciesText {
  const char* name;       // full name
  const char* shortName;  // fits the polaroid caption line
  const char* origin;
  const char* bio;
  const char* quirk;
  const char* food;       // staple, as a menu line
  const char* treat;
  const char* game;       // minigame name
  const char* hideLine;   // toast after HIDE
  const char* eggHint;    // specimen tray line
  const char* evidence[16];  // 0..5 Hearsay, 6..9 Anecdote, 10..12 Eyewitness, 13..14 Photographic, 15 Irrefutable
  const char* names[6];   // name generator
  const char* souvenir;
  const char* hoax;       // the HOAX reveal
};

static const SpeciesText SPECIES_TEXT[4] = {
  {"Mothman", "Mothman", "Point Pleasant, West Virginia. First reported 1966.",
   "A six-foot moth person with glowing red eyes, which sounds alarming until you realize he is "
   "just very excited to see you. Hobbies include porch lights, rooftops, and being mysteriously "
   "nearby. Now has his own festival, a shiny statue, and no idea how to handle the fame.",
   "Loves light. Too much lamp time and he bonks into the glass.",
   "Moth dust cakes", "Porch-light candy", "Lamplight Waltz",
   "Hiding behind the porch light. The eyes give it away.",
   "Found under a porch light in West Virginia.",
   {"Dusty wing scale", "Feathery antenna tip", "Scorched porch bulb", "Red glint in a window",
    "Rooftop scuff marks", "Fuzzy collar tuft",
    "Neighbor's late-night story", "Moth-shaped dust print", "Festival flyer, signed?", "Bent TV antenna",
    "Sketch by a night-shift trucker", "Two red dots on a doorbell cam", "Tourist's shaky video",
    "Red-eye polaroid", "Silhouette on a water tower", "A clear photo. Nobody believes it."},
   {"Sir Flutters", "Porchlight Pete", "Lampwick", "Dusty", "Gloweyes", "Lamp Boy"},
   "a single dusty wing scale", "It was an owl in a cape the whole time."},
  {"Bigfoot", "Bigfoot", "Pacific Northwest woods. Filmed at Bluff Creek, California, in 1967.",
   "The forest's most dedicated hiker and star of a 1967 film he insists was shot without his "
   "consent. Shoe size: yes. Photos in focus: zero. Enjoys berries, long walks, and leaving exactly "
   "one footprint, just enough to start an argument.",
   "Very camera shy. Hikers with cameras cost extra Mystique.",
   "Trail mix", "Huckleberry tart", "Forage",
   "Behind a tree. Mostly. The feet are sticking out.",
   "Too big for the nest. Smells like pine.",
   {"Muddy footprint", "Snapped branch", "Tuft of brown fur", "Half-eaten huckleberry", "Pine-needle nest",
    "Distant wood knock",
    "Fur tuft on barbed wire", "Camper's missing trail mix", "Rock stack, suspiciously tidy", "Big hand print on a log",
    "Ranger's field sketch", "Thermal-cam smudge", "Hunter's trembling voicemail",
    "Plaster footprint cast", "Out-of-focus film still", "A clear photo. Nobody believes it."},
   {"Big Doug", "Mossfoot", "Gerald", "Stompy McPine", "Harold", "Squatchy"},
   "a giant plaster footprint cast", "It was a guy in a suit. You can see the zipper."},
  {"Nessie", "Nessie", "Loch Ness, Scotland. Rumored for roughly 1,500 years.",
   "Scotland's most famous introvert. Her best-known photo (1934) was later admitted to be a toy "
   "submarine with a fake head, which she maintains was a body double. Damp, polite, and world "
   "class at holding her breath.",
   "Needs her Loch Level topped up. Slow to grow, calm as an adult.",
   "Smoked haddock", "Shortbread finger", "Loch Hop",
   "Submerged. Just a ripple now.",
   "Kept in a bucket of loch water.",
   {"Strange ripple", "Shed scale", "Loch pebble, oddly warm", "Kelp in a knot", "Bubbles, no wind",
    "Wet flipper print",
    "Fisherman's tall tale", "Hump-shaped wake", "Tartan scrap on the shore", "Missing picnic haddock",
    "Monk's old manuscript", "Sonar blip", "Ferry passenger's sketch",
    "Long-neck silhouette photo", "Toy submarine (evidence?)", "A clear photo. Nobody believes it."},
   {"Wee Nessa", "Lady Ripple", "Bubbles", "Morag", "Shortbread", "Wee Humphrey"},
   "a smooth loch pebble", "It was a toy submarine with a sculpted head. Again."},
  {"Fresno Nightcrawler", "Nightcrawler", "Fresno, California. Caught on a security camera in 2007.",
   "A small white someone who is about 90% legs and 10% pants. He is not here to haunt you. He is "
   "just going for a walk. Very calmly. At 2 a.m. Please respect the stroll.",
   "Needs his walks. Fussy about his pants.",
   "One grape, slowly", "Two grapes, recklessly", "Evening Stroll",
   "Standing very still. He is a coat rack now.",
   "It was walking when we found it.",
   {"Tiny footprint", "Lost sock", "Pant thread", "Gate left open", "Grass, very evenly trampled",
    "Gum wrapper, folded neatly",
    "Neighbor saw 'pants walking'", "Lawn sprinkler, avoided", "Pants on the clothesline, moved",
    "Sock, other one",
    "Night-shift guard's notes", "Grainy CCTV still", "Doorbell cam: two legs, no fuss",
    "Yosemite trail-cam frame", "Security footage, 2007", "A clear photo. Nobody believes it."},
   {"Slacks", "Trouser Tim", "Longshanks", "Pantaloon", "Stilts", "Gerald"},
   "one lost sock", "It was two pool noodles in pants."},
};

static const char* const PANTS[4] = {"Corduroy", "Pinstripe", "Pajama", "Cargo"};
static const char* const TIER_NAME[5] = {"Hearsay", "Anecdote", "Eyewitness", "Photographic", "Irrefutable"};
static const char* const STAGE_NAME[7] = {"Egg", "Whisper", "Rumor", "Sighting", "Legend", "Blurry Photo", "Hoax"};
static const char* const STAMP_WORD[7] = {"UNHATCHED", "UNVERIFIED", "UNVERIFIED", "SIGHTED", "CONFIRMED", "INCONCLUSIVE", "DEBUNKED"};
