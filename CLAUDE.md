# CRYPTLINGS

A cozy, gently creepy virtual pet for the Waveshare ESP32-C6-Touch-LCD-1.47. At first boot the player picks one of four cryptid eggs (Mothman, Bigfoot, Nessie, the Fresno Nightcrawler) and raises it to an adult. Cryptids are nocturnal and run on the real clock. Well raised adults become a LEGEND, neglected ones a BLURRY PHOTO, spoiled ones a HOAX. Nothing dies. Sibling project of NETSPAWN (same board, same engine pattern), but the theme is a cryptozoologist's field journal at midnight, never techno and never horror.

## House rules

- Never use em dashes (or en dashes) in code comments, docs, UI copy, or commit messages. Use commas, colons, parentheses.
- Direct care is always enough to keep the pet healthy. Nothing required may depend on radio, network or location. The optional "Witness Sense" (a receive-only count of nearby Bluetooth advertisers, default OFF) is flavor only and is not built yet.
- No death state. Neglect ends in the BLURRY PHOTO adult.
- Evolutions only happen live inside the evening window (20:00 to 23:00 local), never during boot catch-up.
- Keep the engine free of Arduino and IO so it builds on the host.

## Design docs

Full plan and brief live in the owner's Obsidian vault at `01 Projects/CRYPTLINGS/`:
- `CRYPTLINGS_Plan_v1.md`: concept, the four cryptid bios, hardware, architecture, phases.
- `CRYPTLINGS_Brief_v1.md`: Part A visual identity (palette tokens, fonts, sprite sizes and I4 budgets, animation beats, chrome, boot text). Part B mechanics (stats, schedule, quirks, lifecycle and fork formula, evidence, time handling, buzzer motifs, HUD pixel budget). Section numbers in code comments (B4, B7) refer to Part B.
- `CRYPTLINGS_Sprites_v1.png`: the chibi art sheet (4 species x 6 forms). The source SVG art is a JS module from the web mockup; ask the owner for the artifact link if you need it.

## Hardware

Waveshare ESP32-C6-Touch-LCD-1.47: 172 x 320 IPS (JD9853, SPI), AXS5106L capacitive touch (I2C, single touch), ESP32-C6 RISC-V 160MHz, 512KB SRAM, NO PSRAM, 8MB flash, microSD, LiPo via battery module, passive buzzer. IMU depends on the board variant, feature-detect it (it enables real steps for the Nightcrawler's walk). No RTC: NTP on boot when WiFi creds exist, else last saved time, flagged UNVERIFIED.

## Layout

```
include/cryptlings_core.h   engine API, Tuning (every number TUNABLE), POD State
src/cryptlings_core.cpp     engine
src/main.cpp                P1 firmware: serial console driving the engine, NVS save
host_test/test_host.cpp     host tests + balance matrix (not built by PlatformIO)
host.mk                     host build: make -f host.mk test (rename to Makefile if you like)
platformio.ini              pioarduino platform (Arduino core 3.x, needed for the C6)
```

## Build

- Host tests: `make -f host.mk test` (verbose stat trace: `make -f host.mk trace`). Requires g++ or clang++ with C++17.
- Firmware: `pio run -t upload && pio device monitor`. For NTP add to build_flags: `-DWIFI_SSID=\"name\" -DWIFI_PASS=\"pass\"`. Time zone defaults to America/New_York (`TZ_RULE`).
- Serial console keys: `?` help, `!` then `1..4` new egg (replaces the pet), `R` release an adult, `f` feed, `t` treat, `p` pat, `c` cuddle, `n`/`g` play, `h` hide, `x` clean, `l` lights, `w`/`W` fast-forward an hour/day, `s` status.

## Status: P1 (core engine) done, validated on host

What the engine covers: five stats (Belly, Cheer, Rest, Mystique, Tidy), nocturnal schedule with dawn tuck-in and owner quiet hours (23:00 to 07:00, halved decay), day-wake penalty, exposure and Mystique, all care actions with cooldowns, the four species quirks (Mothman lamp and dizziness, Bigfoot hikers and foraging, Nessie loch level, Nightcrawler walks and pants), lifecycle with real moon phase and full-moon early evolution, the LEGEND / BLURRY / HOAX fork from running aggregates, evidence rolls with five folklore tiers and corroboration counts kept across pets, 72h capped catch-up at half decay with a floor of 10, hibernation and dig-out, release, and crc-checked persistence. A fresh-eyes code review found 10 issues (mostly time math: forward-only night rollover, clock set back, hibernation not aging the pet, catch-up floor vs nightly penalties, DST in the console, tuck-in window, evidence grinding caps, hiker alert timing, console safety); all are fixed, and the engine-side ones (rollover, clock-back, hibernation age, catch-up floor, tuck-in, evidence caps) have regression tests.

Console keys `!` then `1..4` start a new egg (replaces the pet), `R` releases an adult. main.cpp passes a syntax check against stub Arduino headers but has NOT been compiled with the real toolchain or run on the board yet. First thing on hardware: `pio run`, fix any core 3.x API drift, then raise a pet over serial.

### Balance results (host, 20 simulated days, `make -f host.mk test`)

| Keeper | What they do | Outcome |
|---|---|---|
| good | 5 short evening sessions + dawn tuck-in | LEGEND every species, score about 90 |
| casual | 2 evening sessions doing full care, tuck-in every other day | LEGEND (score 80 to 83), except Bigfoot: BLURRY (56) because unanswered hikers eat Mystique. Intentional: Bigfoot is the Mystique challenge pet |
| lazy | 1 quick session a night, never hides | BLURRY (25 to 30) |
| neglect | feeds every other day | BLURRY (13 to 16), grows up late (about day 11, Nessie 14) |
| spoiler | good care plus 3 treats a night | HOAX |

Adults arrive on day 8 (evening of), Nessie day 10. Tuning changes made during balancing, vs the brief: staple meal 40 (was 30), cheer awake decay 3 (was 4), tidy awake 2.5 (was 3), normal play 25 (was 20), Mystique base decay 1.0 (was 0.5) and quiet-night bonus 0.75 (was 2) so hiding actually matters, and `lateGrowMul` (a stage past 2x its time grows up even if its exit condition fails, so neglected pets are never stuck). Update the brief if these stick.

## Next phases

- **P2 art pipeline:** `tools/bake_sprites.py` per Brief A4.6. SVG to I4 (16-color indexed), fixed per-species palette order, four time-of-day palette LUTs plus `pal_blurry` and `pal_spotted`, manifest JSON on SD, contact sheet PNG. Translucent bits in the mockup art (shadows, blush, glow) must be flattened.
- **P3 UI shell:** bring up JD9853 + AXS5106L with LVGL (partial buffers), Field Journal tokens, folder-tab pager (DEN, CASE FILE, FIELD KIT, ALMANAC, SETTINGS), DEN layout per Brief B11, setup flow per B3.
- **P4 care UI + the four minigames (B5).** **P5 evolution reveal, Case File, release flow.** **P6 polish:** fog, fireflies, buzzer motifs (B10), enclosure.

## Open questions (owner)

- Project name (CRYPTLINGS is a working title).
- Fixed dusk/dawn vs real sunset from a latitude setting.
- Should casual Bigfoot owners get more slack on hikers (lower `hikerHit`)?
- Should NETSPAWN and CRYPTLINGS share one engine library? The NETSPAWN repo currently holds only its header.
