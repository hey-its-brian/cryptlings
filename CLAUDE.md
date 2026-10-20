# CRYPTLINGS

A cozy, gently creepy virtual pet for the Waveshare ESP32-C6-Touch-LCD-1.47 and (since v1.1 work) the M5Stack StopWatch round AMOLED. At first boot the player picks one of four cryptid eggs (Mothman, Bigfoot, Nessie, the Fresno Nightcrawler) and raises it to an adult. Cryptids are nocturnal and run on the real clock. Well raised adults become a LEGEND, neglected ones a BLURRY PHOTO, spoiled ones a HOAX. Nothing dies. Sibling project of NETSPAWN (same board, same engine pattern), but the theme is a cryptozoologist's field journal at midnight, never techno and never horror.

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

M5Stack StopWatch (second target, `-DCRYPT_ROUND=1`, env `stopwatch`): ESP32-S3R8 (8 MB PSRAM, 16 MB flash), 466 x 466 round AMOLED (CO5300, QSPI) with CST820 touch, side keys A (G2) and B (G1), power button on the M5PM1 power chip (read from PM1 reg 0x48; single-press reset disabled via reg 0x49 bit 0; PM1 GPIO1 routed as IRQ to G12 to wake light sleep), ES8311 speaker, vibration motor, RX8130 RTC (keeps UTC; trusted once set by NTP or by hand, NVS key `rtcOk`), BMI270 IMU. M5Unified 0.2.27 + M5GFX 0.2.34 do the hardware. Controls, motion, battery and the hardware-unchecked list: docs/STOPWATCH.md.

## Layout

```
include/cryptlings_core.h   engine API, Tuning (every number TUNABLE), POD State
src/cryptlings_core.cpp     engine
src/app/                    portable UI: gfx renderer, flow (app.cpp), minigame rules (app_game.cpp), sound cues, motion
src/app/target.h            CRYPT_ROUND 0 = 172x320 rectangle (C6), 1 = 466x466 round (StopWatch)
src/app/layout.h            all geometry per target (screens, lists, minigame rules)
src/app/app_draw.cpp        rectangle drawing + hitAt(); app_draw_round.cpp is the round one. app.cpp never uses coordinates, only HitId
src/assets/                 generated: fonts.h / fonts_round.h (4bpp), art.h / art_round.h (16-color sprites, scenes, icons)
src/hal/board.*             C6 board: display, touch, backlight, buzzer, battery, BOOT button
src/main.cpp                C6 firmware: wires the app to the board, NVS, clock, debug console
src/main_stopwatch.cpp      StopWatch firmware: M5Unified glue, power button via M5PM1, speaker queue, RTC, IMU axes (ACC_X/Y/Z flags)
sim/sim_main.cpp            desktop simulator with scripted touches, writes screenshots
host_test/test_host.cpp     engine tests + balance matrix
tools/                      bake_fonts.py, art/render.js + bake_sprites.py (regenerate assets)
host.mk                     host builds: test, trace, sim
docs/HARDWARE.md            C6 pins, init table, touch protocol, buzzer wiring, sources
docs/STOPWATCH.md           StopWatch build, controls, motion, battery, what is unchecked on hardware
platformio.ini              env esp32-c6 (default) and env stopwatch, both on pioarduino 55.03.312 (Arduino core 3.x); mixing in espressif32 6.x breaks alternate builds
```

## Build

- Host tests: `make -f host.mk test` (verbose stat trace: `make -f host.mk trace`). Requires g++ with C++17.
- Simulator: `make -f host.mk sim` writes about 65 screenshots of every screen and flow to `build/shots/` (PPM). `make -f host.mk sim_round` does the round layout into `build/shots_round/` (its script finds controls through `hitAt`, fakes the motion sensor, and has regression checks for face down, real-step walks, side keys, burrow dig-out and the hiker vibration). Use them to check UI changes before flashing. A refactor of shared code should leave the rectangle shots pixel-identical.
- Regenerate art: `node tools/art/render.js [--round]` (needs Playwright + Chromium) then `python3 tools/bake_sprites.py [--round]`. Fonts: `python3 tools/bake_fonts.py [--round]` (needs Pillow).
- Firmware: C6 `pio run -e esp32-c6 -t upload && pio device monitor`; StopWatch `pio run -e stopwatch -t upload`. For NTP add to build_flags: `-DWIFI_SSID=\"name\" -DWIFI_PASS=\"pass\"`. Time zone defaults to America/New_York (`TZ_RULE`).
- Everything is played on the touch screen. Serial console (debug only): `s` status, `w`/`W` fast-forward an hour/day, `S` save, `?` help; StopWatch also `a` (accelerometer in screen terms).

## Status: v1.0.0 released, playable on the device (all phases through P6 core)

P1 engine: done, validated on host and on the board (serial console). Code review fixes applied, regression tests in host_test.

UI firmware (written 2026-10-19, builds with pioarduino 55.03.312 / Arduino core 3.3.12: RAM 42%, flash 29%):
- Screens: boot case-file typewriter, specimen tray (4 eggs, swipe or arrows, hold to incubate), name picker, DEN (polaroid scene per species with real moon phase, fog, fireflies, day/night, five stat pips, hint line, best-action + CARE buttons), CARE list (species rows: lamp, forage, loch, walk, pants), FILE (bio, quirk, photo record, evidence with tiers and corroboration, release), MORE (almanac, sound, brightness, screen timeout, set clock, start over).
- Overlays: hatch/evolution polaroid develop + stamp, fork card (care, hidden, rough nights), Bigfoot hiker alert with 10 s HIDE, burrow dig-out, pants picker, clock setter, start-over confirm, farewell.
- Four 20 s minigames: Lamplight Waltz (drag), Forage (tap), Loch Hop (tap), Evening Stroll (alternate taps, also the walk).
- Sound motifs per Brief B10 on an external buzzer (GPIO17), quiet hours, 6 unattended sounds a day, need hoot at most every 45 min.
- Screen sleeps after the chosen timeout. Battery saver (MORE tab, default on): display sleep, touch held in reset, processor light sleep with a 60 s timer wake. Wake button on GPIO7 (tap: wake or lights out; 3 s hold: soft power off to deep sleep, ext1 wake). Battery read once a minute awake, never asleep; two empty reads trigger soft power off. IMU disabled at boot. Saves on events, every 10 min, on screen off and before power off.
- Art: the mockup's SVG chibis rasterized to 16-color sprites (4 species x 6 forms x 5 moods), 16 scene backgrounds, icons. Fonts baked 4bpp. HOAX reuses the Legend art with a zipper.
- A second fresh-eyes review found 13 issues (collection lost after release + reboot, hand-set clock skipping catch-up, hiker HIDE on cooldown, screen timeout gaps, result card dismissed by the ending tap, holding the egg, deferred hiker/reveal overlays, reveal dismissed early, stale scroll, owner-set clock flag, LEDC timer sharing, settings size migration); all fixed, the two serious ones have sim regression checks.

Flashed and played on the board 2026-10-19: display, touch, buzzer, battery and sleep all work. Public repo github.com/hey-its-brian/cryptlings, release v1.0.0 (full image for 0x0, app image for 0x10000, player's guide).

StopWatch port (2026-10-20, not yet flashed; a fresh-eyes review found 10 issues, all fixed: key wake also changed the tab, RTC ignored because of the latched VBLF flag, PM1 IRQ line setup, face down needing a real flip, power off fallback, missed quick power taps, IMU no-new-sample dropouts, burrow label outside its hold target, speaker left powered): round layout for every screen, overlay and minigame (scene fills the circle, holds are rings, Nessie's loch and the game timer on the rim, one action button, tabs as pills in the sky), round art (200 px creatures, 466 px scenes with the horizon at y 280, dithered) and fonts (about 1.5x). Platform gained optional `vibrate()` and `accel()`; App gained `hitAt()` (semantic hit ids), `onButton()` (side keys) and `motionTick()` (face down 1.5 s = lights out, real steps during the walk, tilt in the Waltz). Builds: StopWatch flash 55% (3.6 MB, mostly art), RAM 7% (framebuffer in PSRAM); C6 unchanged (RAM 42%, flash 29%) and its sim screenshots stayed pixel-identical through the refactor.

Not done / next:
- Frame rate: try `-DLCD_SPI_HZ=80000000` (Waveshare's IDF demo runs 80 MHz).
- StopWatch hardware checks: colour byte order (`-DFB_SWAP_BYTES`), IMU axes (`ACC_X/Y/Z`), power-button wake from light sleep (PM1 IRQ on G12), speaker volume. See docs/STOPWATCH.md.
- Not built: motion on the C6 (its QMI8658 is still switched off; the app side is ready through `Platform::accel`), Witness Sense, custom name keyboard, SD card. The power LED (LED2 via R19) is hardwired; only removing R19 turns it off.
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
