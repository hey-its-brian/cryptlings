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

Waveshare ESP32-C6-Touch-LCD-1.47: 172 x 320 IPS (JD9853, SPI), AXS5106L capacitive touch (I2C 0x63, single touch), ESP32-C6 RISC-V 160MHz, 512KB SRAM, NO PSRAM, 8MB flash, microSD (unused), LiPo via battery module (ADC on GPIO0), QMI8658A IMU always present (unused so far). The board has NO buzzer: an external passive buzzer goes on GPIO17. An external momentary button on GPIO7 wakes the screen, taps it off, and a 3 s hold is a soft power off (save + deep sleep; GPIO7 is the only free deep-sleep wake pin). No RTC: NTP on boot when WiFi creds exist, else last saved time, flagged UNVERIFIED (settable on the MORE tab). Pins, init table and touch protocol: docs/HARDWARE.md.

## Layout

```
include/cryptlings_core.h   engine API, Tuning (every number TUNABLE), POD State
src/cryptlings_core.cpp     engine
src/app/                    portable UI: gfx renderer, every screen, minigames, sound cues
src/assets/                 generated: fonts.h (4bpp fonts), art.h (16-color sprites, scenes, icons)
src/hal/board.*             display, touch, backlight, buzzer, battery, BOOT button
src/main.cpp                firmware: wires the app to the board, NVS, clock, debug console
sim/sim_main.cpp            desktop simulator with scripted touches, writes screenshots
host_test/test_host.cpp     engine tests + balance matrix
tools/                      bake_fonts.py, art/render.js + bake_sprites.py (regenerate assets)
host.mk                     host builds: test, trace, sim
docs/HARDWARE.md            pins, init table, touch protocol, buzzer wiring, sources
platformio.ini              pioarduino platform (Arduino core 3.x, needed for the C6)
```

## Build

- Host tests: `make -f host.mk test` (verbose stat trace: `make -f host.mk trace`). Requires g++ with C++17.
- Simulator: `make -f host.mk sim` writes about 60 screenshots of every screen and flow to `build/shots/` (PPM). Use it to check UI changes before flashing.
- Regenerate art: `node tools/art/render.js` (needs Playwright + Chromium) then `python3 tools/bake_sprites.py`. Fonts: `python3 tools/bake_fonts.py` (needs Pillow).
- Firmware: `pio run -t upload && pio device monitor`. For NTP add to build_flags: `-DWIFI_SSID=\"name\" -DWIFI_PASS=\"pass\"`. Time zone defaults to America/New_York (`TZ_RULE`).
- Everything is played on the touch screen. Serial console (debug only): `s` status, `w`/`W` fast-forward an hour/day, `S` save, `?` help.

## Status: playable device firmware (all phases through P6 core), not yet flashed with the UI

P1 engine: done, validated on host and on the board (serial console). Code review fixes applied, regression tests in host_test.

UI firmware (written 2026-10-19, builds with pioarduino 55.03.312 / Arduino core 3.3.12: RAM 42%, flash 29%):
- Screens: boot case-file typewriter, specimen tray (4 eggs, swipe or arrows, hold to incubate), name picker, DEN (polaroid scene per species with real moon phase, fog, fireflies, day/night, five stat pips, hint line, best-action + CARE buttons), CARE list (species rows: lamp, forage, loch, walk, pants), FILE (bio, quirk, photo record, evidence with tiers and corroboration, release), MORE (almanac, sound, brightness, screen timeout, set clock, start over).
- Overlays: hatch/evolution polaroid develop + stamp, fork card (care, hidden, rough nights), Bigfoot hiker alert with 10 s HIDE, burrow dig-out, pants picker, clock setter, start-over confirm, farewell.
- Four 20 s minigames: Lamplight Waltz (drag), Forage (tap), Loch Hop (tap), Evening Stroll (alternate taps, also the walk).
- Sound motifs per Brief B10 on an external buzzer (GPIO17), quiet hours, 6 unattended sounds a day, need hoot at most every 45 min.
- Screen sleeps after the chosen timeout. Battery saver (MORE tab, default on): display sleep, touch held in reset, processor light sleep with a 60 s timer wake. Wake button on GPIO7 (tap: wake or lights out; 3 s hold: soft power off to deep sleep, ext1 wake). Battery read once a minute awake, never asleep; two empty reads trigger soft power off. IMU disabled at boot. Saves on events, every 10 min, on screen off and before power off.
- Art: the mockup's SVG chibis rasterized to 16-color sprites (4 species x 6 forms x 5 moods), 16 scene backgrounds, icons. Fonts baked 4bpp. HOAX reuses the Legend art with a zipper.
- A second fresh-eyes review found 13 issues (collection lost after release + reboot, hand-set clock skipping catch-up, hiker HIDE on cooldown, screen timeout gaps, result card dismissed by the ending tap, holding the egg, deferred hiker/reveal overlays, reveal dismissed early, stale scroll, owner-set clock flag, LEDC timer sharing, settings size migration); all fixed, the two serious ones have sim regression checks.

Not done / next:
- First flash and hardware shake-down: display colors and offset, touch mapping, buzzer level, battery reading, frame rate (try `-DLCD_SPI_HZ=80000000`).
- Not built: real-step walks from the IMU, Witness Sense, custom name keyboard, SD card. The power LED (LED2 via R19) is hardwired; only removing R19 turns it off.
- Brief's tuning numbers changed during balancing (see below); update the brief if they stick.

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
