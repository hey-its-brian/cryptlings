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
| BOOT button | 9 | Active low. Wakes the screen in this firmware |
| Buzzer | **7 (external)** | The board has **no buzzer**. See below |

## The buzzer (you add it)

The board has no speaker or buzzer. To hear the owl hoots, chimes and the evolution
phrase, wire a small **passive** piezo buzzer:

- Buzzer `+` to **GPIO7**, header H1 pin 22 (bottom right of the 22-pin header).
- Buzzer `-` to **GND**, header pin 3, 4 or 6.
- Optional: a 100 ohm resistor in series to soften it.

Use a *passive* buzzer (the kind that needs a tone signal). An active buzzer only beeps
at one pitch. To use a different pin, add `-DBUZZER_PIN=<n>` to `build_flags`, or
`-DBUZZER_PIN=-1` for no sound at all. Avoid GPIO8 and GPIO9 (boot strapping pins).

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
