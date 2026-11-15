# RT-1000 Pro stock firmware: display and graphics subsystem

Source: `out/dec_named.c` (Ghidra), `out/fw.dis`, `out/fw.bin` (base 0x08002800).
Confidence: **H** = read directly from code, **M** = strong inference, **L** = guess.

## 1. LCD controller and interface

### 1.1 Bus: SPI3, 1-wire TX-only, hardware CS (H)
| Signal | Pin | Evidence |
|---|---|---|
| SCK | PB3, AF6 (SPI3_SCK) | `FUN_08018f80`: gpio_init(GPIOB, 0x28, mux), `gpio_pin_mux_config(GPIOB,3,6)` |
| MOSI/SDA | PB5, AF6 (SPI3_MOSI) | same: `gpio_pin_mux_config(GPIOB,5,6)` |
| CS | PA15, AF6 (SPI3_CS, hardware NSS output) | `FUN_08018f80`: GPIOA 0x8000 mux, `gpio_pin_mux_config(GPIOA,15,6)`; `FUN_08027f4c(SPI3,1)` sets ctrl2.hwcsoe |
| DC/RS | PC13 (low = command) | `FUN_080190b4`: clr PC13 (0x40020828=0x2000), send, set PC13 (0x40020818) |
| RST | PC14 (active low) | `FUN_08018d68` start: set, 1 ms, clr, 1 ms, set, 120 ms |
| Backlight | PA4 = DAC1 output (analog), see section 3 | `FUN_08021e04`, `FUN_08012514` (PA4/PA5 set to analog) |

PC13 and PC14 are push-pull outputs, both driven low right after `FUN_08018f80` configures them.

SPI3 setup (`FUN_08018f80` -> `spi_init` `FUN_08027f86`), with the init struct in AT32 `spi_init_type` field order:
- transmission_mode = 3 (half-duplex TX: ctrl1 slben and slbtd set, so one line, transmit only)
- master
- mclk_div = 0 (/2 of the APB1 clock)
- MSB first
- 8-bit frames
- CPOL = 1, CPHA = 0. Caveat: this rests on a field-order mapping (struct byte 5 -> ctrl1 bit1, byte 6 -> bit0) (M).
- hardware CS
- SPI enabled by `FUN_08027f42`

No DMA is used for the LCD. The only DMA channel the code sets up (`FUN_0800836c`) serves the ADC (periph addr 0x4001204C).

Low-level routines (H):
| Addr | Proposed name | Behaviour |
|---|---|---|
| 0x080190d8 | `lcd_spi_write8(u8)` | write SPI3->dt, spin on sts & 0x02 (TDBE) |
| 0x080190b4 | `lcd_write_cmd(u8)` | DC low -> write8 -> DC high |
| 0x08019116 | `lcd_write_u16(u16)` | write8(hi), write8(lo). This is also the pixel write. |
| 0x080190f8 | `lcd_write_buf_inv(u8*buf, int n)` | sends `~buf[i]` for each byte |

Implementation note: the stock code raises DC as soon as TDBE is set, while the last command bit may still be shifting out. An open firmware should wait for `!BSY` before it toggles DC.

### 1.2 Controller: GalaxyCore GC9307 (or a GC9306-family part), 240x320 (M)
The sequence starts with GalaxyCore's inter-register enable (0xFE, 0xEF) and uses vendor registers 0xE6, 0xE7, 0xF0 and 0xF1 (gamma). It ends with 0xFE, 0xFF. This matches published GC9307 init code.

Init sequence `FUN_08018d68` (called from main init `FUN_08015158` and from boot `FUN_08012f50`). C = command, D = data bytes (H):
```
RST: PC14=1, 1ms, PC14=0, 1ms, PC14=1, 120ms
C FE
C FE
C EF                     ; inter-register enable
C B3  D 03
C B6  D 01
C A3  D 11
C 21                     ; INVON (display inversion ON)
C 36  D E0               ; MADCTL = MY|MX|MV, RGB order
C 3A  D 05               ; COLMOD 16 bpp RGB565
C B4  D 21
C B1  D C0
C E6  D 50 43
C E7  D 38 43
C F0  D 0C 46 25 56 AC 24 25 00 00 12 15 16 17 0F   ; gamma +
C F1  D 00 26 25 3A B9 0F 10 00 00 07 07 17 16 0F   ; gamma -
C 35  D 00               ; TE on
C 44  D 00               ; tear scanline (only one byte is sent)
C FE
C FF
C 11                     ; sleep out
delay 120ms
C 29                     ; display on
C 2A  D 00 00 01 3F      ; 0..319
C 2B  D 00 00 00 EF      ; 0..239
C 2C
76800 (0x12C00) x write_u16(~0)  ; FUN_08018d46(0): clear to black
```
The function also sets globals: `0x20000860` = 0x0000 (default background), `0x20000862` = 0xFFFF, `0x20000864` = 0x8410 (gray).

### 1.3 Resolution, orientation and pixel format (H/M)
- The logical frame is **240 (x) x 320 (y), portrait**. Full-screen clears are `fill(0,0xEF,0,0x13F)`, and 0x12C00 = 240*320 (H).
- **Window routine** `0x08019078 lcd_set_window(x0, x1, y0, y1)` sends `0x2B` with x0,x1 (16-bit each) and `0x2A` with y0,y1, then `0x2C`. With MADCTL = 0xE0, **x goes to RASET (0x2B) and y goes to CASET (0x2A)** (H).
- Pixel stream order inside a window is **column-major: y runs fastest, then x**. Every blitter loops x outer and y inner (H).
- **y = 0 is the bottom edge and y increases upward** (M/H):
  - Text wrap moves to the next line by y -= 28 and restarts at y = 0x124 at the top (`FUN_080096f2`).
  - "Starting up..." is drawn at y = 0xA0 and "Please wait" at y = 0x78, below it.
  - The status bar is filled at y = 0x129..0x13F (`FUN_080186f4`).
  - Font glyphs only render upright if the first pixel sent in each column is the bottom pixel.
- x = 0 is the left edge (text advances with +x).
- **Pixel format is RGB565, sent big-endian (high byte first)** (H).
- **Colour inversion:** the panel runs with INVON (0x21), and every routine sends `~colour`. The net effect is that the colour constants in the code are true RGB565: 0x07E0 green, 0x001F blue, 0xF940 orange-red, 0xFC83 orange, 0x6B4D gray, 0xFFFF white, 0 black. A raw `write_u16(0xFFFF)` (no `~`), which the icon routines use for "off" pixels, therefore shows **black** (H).

To reproduce on an open firmware, either keep INVON and send `~c`, or drop 0x21 and send `c`.

## 2. Drawing primitives (all H unless noted)

All coordinates are in the logical frame above (x 0..239 left to right, y 0..319 bottom to top). "col" means colour (RGB565).

| Addr | Signature | Notes |
|---|---|---|
| 0x08019078 | `set_window(x0,x1,y0,y1)` | 2B / 2A / 2C |
| 0x08018cfc | `fill_rect_xy(x0,x1,y0,y1,col)` | inclusive bounds; writes `~col` |
| 0x08018b08 | `fill_rect(x,y,w,h,col)` | wraps cfc. A 1x1 call is used as draw-pixel and there is no separate pixel routine (M). |
| 0x08018cd0 | `fill_rect_hw(x,y,h,w,col)` | same as b08 with h and w swapped (used for vertical bars) |
| 0x08018bb2 | `draw_frame(x0,x1,y0,y1,thick,col)` | 4 filled bars |
| 0x08018d46 | `fill_screen(col)` | 0x2C plus 76800 pixels; uses the current window |
| 0x08016f94 | `blit_fullscreen_from_spiflash(addr)` | window 240x320, reads 75 x 2 KB from external SPI flash (SPI2, `FUN_08028150` = read 0x03) into 0x20003F16 and sends with `write_buf_inv`. Boot logo is at **ext-flash 0x100000**: raw 153600 B RGB565 big-endian, column-major, bottom-up. |
| 0x08018b34 | `draw_icon72(x,y,bitmap,col)` | 72x72 1bpp, 9 B per column, MSB first; off pixels raw 0xFFFF (black). Used with 0x0802D376 by `FUN_08018ac0`. |
| 0x0800918c | `draw_bitmap24(x,y,u32*cols,w,fg,bg)` | w columns, 24 px tall, one u32 per column, mask 0x800000 first. Menu tab icons at 0x0802D7A8, D808, D868, D928, D988, DAA8 (24 wide). |
| 0x080088a8 | `draw_bitmap15(x,y,w,u16*cols,flag)` | 15-tall, u16 per column, mask 0x8000; fg 0xF940 |
| 0x08009b78 | `draw_status_icon(x,w,u16*cols,on,col)` | 16-tall at y = 0x129..0x138 (status bar), bg 0x94B2 |
| 0x08008a88, 0x08008e34, 0x080095d8, 0x08009bf4 | small fixed icons | 8x16 at 0x0802CDBE; 8x16 copied from 0x08028A78; 8x8 arrow immediate; 16x24 at 0x0802DCD8 |
| 0x08009200 | `draw_big_digit(x,y,ch,tint)` | colour bitmaps, see below |
| 0x08009360 | `draw_big_number(x,y,str,len,tint)` | advance 32 px, a '.' takes 16 px |
| 0x08008b1c, 0x080093a2, 0x08018c14 | bar graphs (battery/RSSI/scroll bar) | built from fill_rect |

### 2.1 Text
Shared glyph conventions (H):
- One glyph is drawn as one window of exactly glyph width x height.
- Data is **column-major**: one word per column, columns left to right.
- Within a column the **first tested bit (MSB of the used field) is the bottom pixel**, and later bits go upward.
- Set bit -> `~fg`, clear bit -> `~bg`.
- Character index = `ch - 0x20` (ASCII from space) unless noted otherwise.

| Draw char | String fn (advance) | Size WxH | Glyph data | Bytes/glyph | Column word decoding |
|---|---|---|---|---|---|
| 0x08008934 | 0x0800967a (+7) | 7x14 | MCU 0x0802C26E | 14 | 7 x u16 LE; test 0x4000 first, 14 bits |
| 0x08008bbc | 0x08009644 (+8) | 7x11 | MCU 0x0802C7A0 | 14 | 7 x u16 LE; test 0x0800 first, 11 bits |
| 0x08008c40 | 0x080096b8 (+10) | 9x15 | MCU 0x0802BE48 | 18 | 9 x u16 LE; test 0x8000 first, 15 bits |
| 0x08008db0 | 0x0800980c (+4) | 3x7 | MCU 0x0802CADA (index ch - '0') | 3 | 1 byte per column, 0x80 first, 7 bits |
| 0x08008cc8 | 0x080097d8 (+17) | 16x24 | MCU 0x0802BA08 | 64 | 4 B per column, 24-bit value = `(b3&0xF)<<20 \| b2<<12 \| b1<<4 \| b0>>4`, test 0x800000 first |
| 0x080089e0 | (via 0x080096f2) | 12x24 | **external SPI flash 0x19C000** | 36 | 12 x 3 B, value = `b2<<16\|b1<<8\|b0`, 0x800000 first |
| 0x08008ea0 | (via 0x080096f2) | 24x24 Chinese (GBK) | **external SPI flash 0x19E000** | 72 | 24 x 3 B, same packing as the 12x24 font |

Notes on individual fonts:
- **16x24 digits (0x0802BA08):** the char map is 0-9, then 'M'=10, 'H'=11, 'z'=12, 'C'=13, '-'=14, '.'=15, ' '=16. Its background comes from global `0x20000860`.
- **24x24 Chinese (ext. flash 0x19E000):** a glyph is selected by GBK code `hi:lo` with `idx = (lo-0x40) + (hi-0x81)*190`, then `idx--` if `lo > 0x7F` (skips 0x7F). This is the standard GBK 0x8140-based layout (21,546 cells x 72 B).

- **Main string routine** `0x080096f2 draw_string(x, y, str, len, fg, bg)` (H):
  - A byte < 0x80 draws a 12x24 ASCII glyph and advances x by 12.
  - 0x00 and 0xFF draw a space.
  - Any other byte >= 0x80 starts a 2-byte GBK code: it draws a 24x24 glyph and advances x by 24.
  - Wrap: when x > 0xDE (ASCII) or x > 0xD2 (GBK), x resets to 6 and y -= 28. If that would underflow, y = 0x124.
- The 7x14 glyphs were checked by rendering them from fw.bin. They come out upright only when flipped vertically, which confirms the bottom-first bit order (H).
- 0x08008f98 and 0x08009064 are menu-header helpers.

### 2.2 Big frequency digits (H)
`0x08009200`: each glyph is a **pre-rendered RGB565 bitmap** (2 bytes per pixel, high byte first, same column-major bottom-up order). The firmware sends `~byte`.
- Digits: 32x40 = 0xA00 B each at MCU **0x08035695**. Index 0-9 = '0'-'9', 10 = 'C', 11 = 'H', 12 = '-'.
- '.': 16x40 = 0x500 B at **0x0803D895**.
- ' ' is skipped.
- When `tint != 0xFFFF`, any non-zero pixel is replaced by the tint colour and zero pixels stay black. With `tint == 0xFFFF` the raw colours are shown.

## 3. Backlight (H for mechanism, M for hardware meaning)
- There is **no PWM timer**. The backlight is driven by **DAC channel 1 (PA4)**:
  - `FUN_0800830c` enables DAC1.
  - Brightness is set by writing 8-bit right-aligned data to `DAC_D1DTH8R` = 0x40007410.
  - PA4 and PA5 are configured as analog in `FUN_08012514`.
  - DAC2 (PA5, 0x4000741C) is enabled separately and is used for something else.
- `0x08021e04 backlight_set(level)`: clamps level to <= 4 and writes `{0x3C, 0x41, 0x46, 0x4C, 0xFF}[level]` (60, 65, 70, 76, 255 out of 255). Values were verified in the literal pool at 0x08021E24.
- 0 = off. The firmware writes 0 at init (`FUN_08015158`), at power-off (`FUN_08022eb8`) and on timeout.
- The brightness setting is byte `settings[0x61]` (settings RAM block at 0x2000378A, loaded from ext-flash 0x2000). The menu handler steps it with `(v+1) % 5`, giving **5 levels (0..4)**.
- `0x0801f790 backlight_wake()`: sets flag 0x20001190 and restores the level.
- `0x080251cc backlight_timeout_tick()`: counts up to the timeout derived from `settings[0x62]` (`FUN_08014b90`, stored in 0x20001114), then writes DAC = 0.
- Unrelated pins: PA13/PA14 (SWD pins) are re-purposed as GPIO outputs in `FUN_08019128` and toggled at power off/recovery. They are not part of the LCD bus; their purpose is unknown (L).

## 4. Boot and display flow (H)
`FUN_08015158` (HW init):
1. GPIO
2. ... SPI2 flash `FUN_080113f8`
3. SPI3/LCD pins `FUN_08018f80`
4. DACs
5. backlight = 0
6. `lcd_init` `FUN_08018d68`

`FUN_08012f50` (startup):
1. Loads settings from ext-flash.
2. Calls `lcd_init` again.
3. Shows either "Starting up..." with `draw_string(0x30, 0xA0, ...)` in colour `settings[0x15]`, or the logo `blit_fullscreen(0x100000)`.
4. Turns the backlight on with `backlight_wake`.
