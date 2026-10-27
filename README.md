# u8g2-linux

[u8g2](https://github.com/olikraus/u8g2) callbacks for Linux userspace,
and `u8g2-oled`, a small tool that draws on a 128x64 I2C OLED.

## Build

It needs the u8g2 library installed as a static library (`libu8g2.a`,
headers in `include/u8g2/`), built with `-ffunction-sections -fdata-sections`:
because the applications are linked with `--gc-sections` they keep only the fonts and
display setups they use.

```sh
meson setup build
ninja -C build
```

## u8g2-oled

```
u8g2-oled [-d i2c] [-a addr] [-r line] [-c controller] [-m] [-n] [text...]
  -d i2c         /dev/i2c-N, or the end of the adapter's device-tree path
  -a addr        7-bit I2C address (default 0x3c)
  -r line        GPIO line name of the display's reset (active low),
                 pulsed at init (default: none)
  -c controller  sh1106 (default) or ssd1306
  -m             centre the text in the middle of the screen
  -n             no I2C: print the frame as text on stdout
```

It draws a frame and up to 5 lines of text, from the top left corner, or
centred with `-m`.

```sh
u8g2-oled -d i2c-mux@70/i2c@3 -r 'display-reset#' Hello world
u8g2-oled -d i2c-mux@70/i2c@3 -r 'display-reset#' -m "Board starting"
u8g2-oled -n -m Hello world   # the same frame, as text
```
