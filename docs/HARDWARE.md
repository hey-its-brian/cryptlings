# Hardware notes: Waveshare ESP32-C6-Touch-LCD-1.47

Everything here comes from Waveshare's official demo code (Arduino and ESP-IDF trees in
`ESP32-C6-Touch-LCD-1.47-Demo.zip`), the official schematic PDF, and docs.waveshare.com.

## Pins

| Function | GPIO | Notes |
|---|---|---|
| LCD SCLK / MOSI / CS / DC / RST | 1 / 2 / 14 / 15 / 22 | JD9853, SPI mode 0, RGB565 big-endian, x offset 34 |
| Backlight | 23 | Active high, PWM (LEDC 5 kHz, 10 bit) |
| SD card MISO / CS | 3 / 4 | Shares SCLK and MOSI with the LCD. The firmware holds CS high (unused) |
| Touch + IMU I2C SDA / SCL | 18 / 19 | 400 kHz |
| Touch RST / INT | 20 / 21 | AXS5106L at 0x63. No firmware upload needed |
| IMU (QMI8658A) | I2C 0x6B, INT 5 / 6 | Present on every board. Not used yet |
| Battery ADC | 0 | 200K/100K divider: battery mV = reading x 3 |
| BOOT button | 9 | Active low. Wakes and sleeps the screen (cannot power off) |
| Wake / power button | **7 (external)** | Momentary button to GND. See below |
| Buzzer | **17 (external)** | Header pin 7 (ESP_RXD). The board has **no buzzer** |

## The wake / power button (you add it)

A small momentary push button between **GPIO7** (header H1 pin 22, bottom right) and
**GND** (header pin 3, 4 or 6). No resistor needed: the firmware uses the chip's own pull-up.

- **Tap** with the screen off: wakes it (instantly, the screen and pet are still in memory).
- **Tap** with the screen on: lights out now.
- **Hold 3 seconds:** soft power off. A "Keep holding" card appears after about a second;
  let go to cancel. The pet and settings are saved to the chip's flash first, then the chip
  goes into deep sleep with everything off. A press wakes it (it reboots, about a second,
  the clock kept running), and the time away is handled like any time off: the pet ages, at
  half the usual decay, up to 72 hours, and longer than that it is found hibernating.

It has to be GPIO7: in deep sleep this chip can only be woken by GPIO0 to 7, and 0 to 6 are
all used (battery reading, screen and SD bus, motion sensor). To use another pin:
`-DWAKE_BUTTON_PIN=<n>` (power off then only works on 0 to 7), or `-DWAKE_BUTTON_PIN=-1`
for none (then touch stays on while the screen is dark so a tap can wake it).

## The buzzer (you add it)

The board has no speaker or buzzer. Wire a small **passive** piezo buzzer:

- Buzzer `+` to **GPIO17**, header H1 pin 7 (ESP_RXD).
- Buzzer `-` to **GND**, header pin 3, 4 or 6.
- Optional: a 100 ohm resistor in series to soften it.

(It used to be GPIO7; that pin now belongs to the wake button.) GPIO17 is UART0's receive
line, unused because the console runs over USB. Use a *passive* buzzer, not an active one.
Other pin: `-DBUZZER_PIN=<n>`, or `-1` for silence. Avoid GPIO8 and GPIO9 (boot pins).

## Battery life

- **Battery saver** (MORE tab, on by default): with the screen dark, the display chip sleeps,
  the touch chip is held in reset, and the processor light-sleeps, waking once a minute to
  update the pet (and hoot if it needs you). A button press wakes it instantly. While USB is
  connected to a computer it skips light sleep so the serial monitor keeps working.
- The battery voltage is read once a minute with the screen on, never while asleep. Two empty
  readings in a row trigger a soft power off to protect the LiPo (the save is kept).
- The motion sensor is switched off at boot (unused).
- **The red power LED cannot be switched off in software.** LED2 is wired from VSYS through
  R19 (3K) to ground, so it is always lit and draws roughly 0.7 mA, which is a big share of the
  sleep current. To stop it, remove R19 (or LED2); look for those silkscreen labels on the board
  (the schematic does not say where they sit physically, so check before desoldering). The charge LED (LED1) only lights while charging and costs
  nothing otherwise.
- Soft power off draws very little: the chip's deep sleep, the battery divider (about 14 uA),
  the regulator and charger, and the power LED unless R19 is removed.
- The SD card is not used (saves live in the chip's flash, which survives power off and even
  an unplugged battery). An inserted card idles on the bus; removing it saves a little more.

## Display init

`src/hal/board.cpp` sends Waveshare's own JD9853 register table (from the IDF
`esp_lcd_jd9853` driver plus `bsp_display_init`): portrait (MADCTL 0x00), RGB order,
inversion on, 65K color. SPI runs at 40 MHz; Waveshare's IDF demo uses 80 MHz, so
`-DLCD_SPI_HZ=80000000` is worth trying for a smoother frame rate.

## Touch

Register 0x01, 14 bytes: point count in the low nibble of byte 1, X = (b2 & 0x0F) << 8 | b3,
Y = (b4 & 0x0F) << 8 | b5. In portrait the X axis is mirrored: x = 171 - raw_x.

## Unconfirmed

- Waveshare's wiki lists I2C addresses 0x51, 0x6B and 0x7E, but both demos use 0x63 for
  touch. If touch never responds (the serial log says `touch NOT FOUND`), run an I2C scan.
- Whether the IMU's step counter is usable (it would let the Nightcrawler's walks count
  real steps). Not used yet.
