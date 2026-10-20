# CRYPTLINGS on the M5Stack StopWatch

The same game as the Waveshare C6 build, laid out for the StopWatch's 466 x 466 round
AMOLED. One codebase: `-DCRYPT_ROUND=1` picks the round layout, art and fonts.

## Build and flash

```bash
pio run -e stopwatch -t upload && pio device monitor -e stopwatch
```

Board settings follow M5Stack's StopWatch quick start (board `esp32s3box`, 16 MB flash, OPI
PSRAM), on the same pioarduino platform as the C6 (Arduino core 3.x), so both environments
share one toolchain. M5Unified 0.2.27 and M5GFX 0.2.34 are
pinned. If the board does not show up for upload, hold the power button for about
2 seconds until the green LED lights (download mode), then upload.

The C6 build is unchanged: `pio run -e esp32-c6` (still the default environment).

## Hardware used

| Part | How |
|---|---|
| Display, CO5300 466 x 466 AMOLED (QSPI) | M5GFX. The app draws a full frame in PSRAM (434 KB) and pushes it whole |
| Touch, CST820 | M5Unified `M5.Touch`. Not read while the screen is dark |
| Side keys A (G2) and B (G1) | Previous / next tab. A press while dark only wakes the screen |
| Power button (on the M5PM1 power chip) | Read from PM1 register 0x48. Tap: screen on or lights out. Hold 3 s: soft power off |
| Speaker (ES8311 + AW8737A) | `M5.Speaker.tone`, the same motifs as the buzzer. Started only when a sound plays, stopped before sleep |
| Vibration motor | `M5.Power.setVibration`: hatch, evolution, hiker alert, need hoot. Follows the SOUND setting and quiet hours |
| RTC, RX8130 | Keeps UTC through power off. Set by NTP or by SET CLOCK; once set, the clock counts as verified |
| BMI270 motion sensor | Face down to sleep, real steps on walks, tilt to steer in the Lamplight Waltz |
| Battery, M5PM1 | `M5.Power.getBatteryLevel`, read once a minute with the screen on, never asleep |

## Controls

- **Power button, tap:** wakes the dark screen, or turns a lit screen off.
- **Power button, hold 3 s (screen on):** soft power off. The "Keep holding" ring runs around the
  rim; let go to cancel. The pet and settings are saved, then the power chip cuts power. The RTC
  keeps time, so the next power on (one press) catches up the time away like any other absence.
  If the chip does not cut power (for example with USB plugged in), the watch goes into deep
  sleep instead: press side key A or the power button to bring it back.
- **From a dark screen,** tap the power button to light it first; a hold is only seen once the
  screen is on.
- **Power button, quick double press:** the power chip's own hard power off (M5Stack's default).
  The firmware saves on every press while it is awake. With the screen dark and battery saver on,
  the last save was at screen off (and every 10 minutes), so at most a few minutes of care are lost.
- **Side keys:** A = previous tab, B = next tab. They also close the pants picker, the start-over
  card and result cards, and step through eggs on the specimen tray.
- **Turn it face down for 1.5 s:** lights out (not during a minigame or the hiker alert). It has to
  be a real flip from face up, so a wrongly mapped sensor can never keep the screen dark.
- **Everything else** is on the touch screen, as on the C6.

## The round layout

The circle is a lens on the cryptid's world. In the DEN the scene fills the whole circle and
the creature stands on the horizon. The time and the four tab pills sit in the sky; the name,
the five needs, the hint and one action button sit on the dark ground below. CARE is a tab
away, so the DEN has a single button.

- Holds draw as rings: hold the egg to incubate, hold the creature to cuddle, hold the burrow to
  dig it out, hold the power button to power off.
- Nessie's loch level runs up the left rim.
- Minigames use the whole circle, with the 20 s timer running around the rim and QUIT at the bottom.
- Scrolling tabs (CARE, FILE, MORE) fade out under the tabs and toward the bottom of the circle.

Screenshots of every screen: `make -f host.mk sim_round` writes them to `build/shots_round/`.

## Motion

| Feature | Detail |
|---|---|
| Face down | Face up (`z > 0.6 g`), then within 5 s `z < -0.75 g` for 1.5 s, turns the screen off |
| Real walks | During the Nightcrawler's WALK, steps from the accelerometer count with the taps. A step is a bump of 0.15 g above the running average, at most one every 300 ms |
| Tilt | In the Lamplight Waltz, with no finger down, tilting slides Mothman around the circle |

The sensor's axes have not been checked against the screen on real hardware. Type `a` in the serial
console to print the reading in screen terms. Lying face up should read `z` near +1; tipping the
right edge down should make `x` negative; tipping the bottom edge down should make `y` negative.
If not, remap with build flags, for example `-DACC_X=(-ay) -DACC_Y=(ax)` (see the top of
`src/main_stopwatch.cpp`).

## Battery

- Battery saver (MORE tab, on by default): with the screen dark the AMOLED sleeps, touch is not
  read, the speaker is stopped and the processor light-sleeps, waking once a minute to tick the
  pet. A side key or the power button wakes it at once. Skipped while USB serial is connected.
- Two empty battery readings in a row (and not charging) trigger a soft power off.

## Not checked on hardware yet

- Colour byte order: the framebuffer is pushed with `setSwapBytes(true)`. If reds and blues look
  wrong, build with `-DFB_SWAP_BYTES=0`.
- The motion axes (above).
- Waking light sleep with the power button relies on the power chip's IRQ on G12 (set up the way
  M5Unified does it for its other PM1 boards). If a power-button press does not wake a dark screen,
  the side keys still do; turning off battery saver also works.
- The RTC: the firmware trusts it unless its "time lost" flag (RX8130 register 0x1D bit 1) is set,
  and clears that flag whenever it writes the time.
- Speaker volume (`M5.Speaker.setVolume(110)` in `soundService`).
