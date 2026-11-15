# RT-1000 Pro stock firmware: storage, serial links and audio

Source: `out/dec_named.c` (Ghidra), `out/fw.bin` (base 0x08002800). Function addresses are given as `FUN_xxxxxxxx`.
Confidence levels: **H** = read directly from code or constants, **M** = strong inference, **L** = guess.

Timer clock note (M): the beep formula only gives the requested frequency if the APB1 timer clock is **48 MHz** (see section 4), so the rates below assume 48 MHz.

---

## 1. External SPI NOR flash

### 1.1 Hardware (H)
| Signal | Pin | Evidence |
|---|---|---|
| CS (active low, software GPIO) | **PB12** | `FUN_080113f8`: gpio_init(GPIOB, 0x1000) output; driver writes 0x1000 to GPIOB clr (0x40020428) / scr (0x40020418) |
| SCK | **PB13** AF5 | `gpio_pin_mux_config(GPIOB,13,5)` |
| MISO | **PB14** AF5 | `(GPIOB,14,5)` |
| MOSI | **PB15** AF5 | `(GPIOB,15,5)` |
| Peripheral | **SPI2** (0x40003800), master, polled, no DMA | `FUN_08027f86/08027f42`, byte xfer `FUN_08027e54`, bulk read `FUN_08027e9c` (sends 0xA5 dummy), bulk write `FUN_08027ee4` |

### 1.2 Command set (H)
| Cmd | Use | Function |
|---|---|---|
| 0x90 + 3x00 | Manufacturer/Device ID; result is stored at 0x20001158 (the low byte is the device ID) | `FUN_080281b0` |
| 0x05 | Read SR1; the busy-wait polls bit0 (WIP) | `FUN_080281f4`, `FUN_080282ac` |
| 0x15 / 0x11 | Read / write SR3 | `FUN_08028220`, `FUN_080283d8` |
| 0x06 | Write enable | `FUN_08028374` |
| 0x03 | Read (any length) | `FUN_08028150(dst, addr, len)` |
| 0x02 | Page program (at most 256 B per call; the caller splits on page boundaries) | `FUN_080280e0`, `FUN_08028394(src,addr,len)` |
| 0x20 | 4 KB sector erase. **The argument is the sector index**, not an address | `FUN_0802824c(sector)` |
| 0xB7 | Enter 4-byte address mode | `FUN_080113f8` |

- If the device ID is 0x18 or 0x19 (W25Q256/512 class), the driver sends a 4th address byte and at init sets SR3.ADP and issues 0xB7 (H).
- The CPS "raw write" command 0xA4 is refused unless the ID is 0x17, 0x18 or 0x19 (`FUN_0801e9a4`).
- The firmware itself only addresses up to **0x3FFFFF**, so the chip is at least 4 MB (32 Mbit). The exact part is not determined (M).
- Read-modify-write helper: `FUN_080282c0(src, addr, len)` (H):
  1. Reads each 4 KB sector into the RAM buffer at 0x20001c0e.
  2. If the target bytes are all 0xFF it programs them directly. Otherwise it erases the sector, merges the new data and reprograms the whole sector.
- All higher-level saves go through `FUN_080282c0`. The pair `FUN_08012408`/`FUN_08012428` wraps flash access; it is probably a pause/resume of audio and IRQs (M).

### 1.3 Memory map (byte addresses in SPI flash)
| Range | Content | Record | Code | Conf |
|---|---|---|---|---|
| 0x000000 (2 KB used) | **Calibration**; valid when byte0 == 0x9A. Per-band tables are 0x70 B (index = band, 5 MHz sub-steps); fields at 0x6B0..0x6EF | 0x800 | `FUN_08014bbc`, `FUN_0801345c`, `FUN_0801367c`, save `FUN_08020770` | H |
| 0x001000 | Calibration **backup** (same layout). If neither copy is valid, the defaults in internal flash at **0x0802AA04** (0x800 B) are used | 0x800 | same | H |
| 0x002000 | **Main settings** (loaded to RAM 0x2000378a); see section 1.6 | 0x400 | `FUN_08012f50`, save `FUN_08020aa0` | H |
| 0x003000 | Settings backup (used when the magic at +0x0C is bad) | 0x400 | `FUN_08012f50`, `FUN_08005438` | H |
| 0x004000-0x00FFFF | **Channels**: 1024 x 0x30 B (index 0..0x3FF) | 0x30 | `FUN_08012ed0`, `FUN_080074b8`, save `FUN_08020a78` | H |
| 0x010000-0x01BFFF | Channel backup | | `FUN_08005438` (backup), `FUN_0801f820` (restore) | H |
| 0x01C100 | **VFO records**: 7 x 0x30 (same layout as channels). Slots 0-2 = Areas A/B/C in band mode 0 (64-999 MHz); slots 3-5 = A/B/C in mode 1 (18-64 MHz); slot 6 = Area C in "ALL BAND" | 0x30 | `FUN_08014e20`, `FUN_08020ccc` | H |
| 0x01C250 | **Radio state** (RAM 0x20003b8a); see section 1.6 | 0x38 | `FUN_08011234`, save `FUN_08020abc` | H |
| 0x01D000 | Backup of sector 0x1C | | | H |
| 0x01E000-0x03DFFF | **Zones**: 250 x 0x208 B | 0x208 | `FUN_0801502c`, `FUN_08015130`, save `FUN_08020d2c` | H |
| 0x03E000-0x05DFFF | Zone backup | | | H |
| 0x05E000-0x091FFF | Written by CPS cmd 0x94 (52 sectors); **never read by this firmware** | | `FUN_0801ce1c` | H (unused) |
| 0x0C6000 (5 sectors), 0x0D0000 (3), 0x0D6000 (1) | Written by CPS cmds 0x95/0x96/0x97; not read | | | H (unused) |
| 0x0F0000 | Table of 0x30-B records indexed by state +0x12, loaded to RAM 0x20009824. Layout: count byte + 4 x 9-B entries, each with a u16 at +7 written to the RF chip (`FUN_0801fc6c`). Possibly band/"Work Range" profiles | 0x30 | `FUN_0801362c`, `FUN_08020adc`, `FUN_08009db8` | M/L |
| 0x0F1000 | Backup of 0xF0000 | | | H |
| 0x0F2000 | **GPS track log**: u16 big-endian count (wraps at 1000), then 1000 x 0x30 starting at 0xF2002 | 0x30 | `FUN_08020c28` (append), `FUN_08013d38`, `FUN_080224ec`, clear `FUN_080081bc` | H |
| 0x0FE000 | **Bluetooth settings** (RAM 0x20003deb). Defaults at 0x0802B7B8: on/off=0, name "UV100" | 0x27 | `FUN_08020a5c` | H |
| 0x0FE400 | **GPS/position settings** (RAM 0x20003bc2). Defaults at 0x0802B7DF: [0] GPS on, [1] GPS baud index, then ASCII lat/lon slots `0000.00000N00000.00000E` | 0x149 | `FUN_08020c08` | H/M |
| 0x0FE800 | **APRS settings** (RAM 0x20003d0b). Defaults at 0x0802B928: callsign "Nocall", paths "Wide1"/"Wide2" for DIGI1/2, beacon timers | 0xE0 | `FUN_08020670` | H |
| 0x0FF000 | Backup of sector 0xFE | | | H |
| 0x100000 | **Boot logo**: 240x320 RGB565, 0x25800 B, streamed to the LCD in 0x800 chunks | | `FUN_08016f94(0x100000)`, CPS 0x9A | H |
| 0x12E000 | **APRS packet log**: u16 big-endian count (wraps at 400), then 400 x 0x100 at 0x12E002. Each entry: 6-B timestamp + 250-B raw AX.25 frame (padded with '~') | 0x100 | `FUN_080206cc`, `FUN_08012cdc`, `FUN_08022034`, clear `FUN_08008180` | H |
| 0x14C000 (24 sectors), 0x164000 (36 sectors) | Written by CPS 0x9C/0x9D; not read | | | H (unused) |
| 0x188000 / 0x198000 | **Chinese pinyin IME tables** (u16 GBK codes; pinyin strings are in fw at 0x08029xxx) | | `FUN_080094e8` | M |
| 0x19C000 | **ASCII font 12x24**: 36 B per glyph, starting at char 0x20 | 0x24 | `FUN_080089e0` | H |
| 0x19E000 | **GBK font 24x24**: 72 B per glyph. index = (hi-0x81)*190 + (lo-0x40), minus 1 if lo > 0x7F | 0x48 | `FUN_08008ea0` | H |
| 0x350000 | Copy of the 8-byte "ID block" | 8 | `FUN_080206b0` | H |
| 0x352000 | **Voice prompts**: 57 clips of 8-bit unsigned PCM at 8 kHz. Index table in fw at **0x080314F1**: 6-B entries {u32 offset from 0x352000, u16 length} | | `FUN_08021fdc` | H |
| 0x3F0000 | CPS 0xA5 region (16 sectors) | | | H |
| 0x3FF000 | "ALL BAND" flag: 3 ASCII bytes "ON " or "OFF"; toggled by boot key code 0xC | 3 | `FUN_080035b4`, `FUN_08020b0c` | H |
| 0x3FF400 | **ID block** (RAM 0x20001098): u32[0] = 0x1122ABCD activation flag (boot key 5, or CPS '4'/0x11 and '4'/0x12); u32[1] = radio ID (at most 99,999,999, set by CPS '5'/0x11) | 8 | `FUN_080035b4`, `FUN_08020690` | H |

Other behaviour:
- **Backing up...** (`FUN_08005438`) copies 0x2000, the channels, 0x1C000, the zones, 0xF0000 and 0xFE000 to their shadow areas.
- **Recovering...** (`FUN_0801f820`/`FUN_0801f828`) copies them back.
- The CPS session end runs backup and then restore (`FUN_0801e9a4`, sub 0xEE).

### 1.4 Channel / VFO record (0x30 bytes)
Sources: `FUN_08004594` (RX setup), `FUN_0800507c` (TX setup), `FUN_08004a68`, `FUN_08012ed0`, defaults at 0x0802B63C.

| Off | Bits | Meaning | Conf |
|---|---|---|---|
| 0x00 | 7:6 | Slot type: 01 = valid/analog ("ANA"), 00 = "DMR" label (not usable here), 11 = empty (0xFF) | H (display strings in `FUN_080221b0`) |
| 0x00 | 5:4 | Checked as "== 1" before APRS TX on the channel (`FUN_08023664`) | L |
| 0x01 | 3:0 | **Scrambler** 0 = off, 1..15 = frequency index. Writes BK4819 reg 0x71 = n*0x408+0x68DC and reg 0x31 bit1 (`FUN_08004bcc`) | H |
| 0x02 | 7:6 | Chooses which calibration gain pair is used on TX (`FUN_080048fc` with 0x20000814 or 0x20000815); used for APRS too. Likely bandwidth wide/narrow | M |
| 0x03 | 2:0 | **Tail tone / STE**: 0 off, 1 = 55 Hz, 2/3/4 = phase-shift codes (BK4819 regs 0x51/0x07/0x52 in `FUN_080051f0`) | M |
| 0x03 | 4:3 | Flag tested ==2 in `FUN_08004a68` and ==1 in `FUN_0800b6a4` (busy lock or talkaround) | L |
| 0x04 | 3:1 | RX squelch-tone sub-mode. Values 0..3 use the CTC/DCS fields below (1 or 3 means inverted DCS lookup); 4 and up use the 24-bit code at +0x1A ("DCS Encrypt" / mute code) | M |
| 0x04 | 5:4 | **Modulation**: 0 = FM, 1 = AM (TX writes DAC2 = AM level), 2/3 = SSB variants (`FUN_08003a74(1/7/5)`, `FUN_08004c28`). Also drives GPIO PB4/PB1 | M/H |
| 0x04 | 7:6 | Chooses the RX calibration set (0x20000816/18 vs 0x20000817/19) and the TX table (+3 vs +10). Likely TX power level / bandwidth | L |
| 0x05 | u32 LE | **RX frequency in 10 Hz units** (43012500 = 430.125 MHz) | H |
| 0x09 | u32 LE | **TX frequency in 10 Hz units** | H |
| 0x0D | u16 LE | **RX tone**: bits 11:0 = code; bits 15:12 = type (0 off, 1 CTCSS in 0.1 Hz, 2 DCS-N, 3 DCS-I) (`FUN_08003d30`) | H |
| 0x0F | u16 LE | **TX tone**, same encoding | H |
| 0x14 | u8 | TX tone sub-mode (counterpart of +4 bits 3:1 for TX) | M |
| 0x16 | u8 | 1 in the defaults; unknown | L |
| 0x1A | u32 | Extended 24-bit code + flag in bit 24 (encrypt / mute code) (`FUN_08003efc`) | M |
| 0x20 | 16 B | **Name**, ASCII, 0xFF-padded | M |

Validity check (`FUN_08012ed0`): type == 1, and both frequencies pass the band check in `FUN_08007a28`.

### 1.5 Zone record (0x208 B)
| Off | Meaning | Conf |
|---|---|---|
| 0x00 | u16 current channel (the RAM copy at 0x20003172 is indexed oddly) | M |
| 0x04 | 16-B name (`FUN_08022700`) | H |
| 0x14 | u16[250] channel list; 0xFFFF or a value >= 0x400 is an empty slot (`FUN_0801502c`) | H |

### 1.6 Main settings (RAM 0x2000378a, 1 KB; defaults at 0x0802B204)
| Off | Meaning | Conf |
|---|---|---|
| +0x0C | u16 magic: 0xABCD = normal; 0x1111 / 0x2222 = special modes (0x20001152 = 1/2; mode 2 = data/repeater loop in `FUN_08027cc8`/`FUN_0801edc0`); anything else = invalid, so load the backup, then defaults | H |
| +0x10 | Show boot image | H |
| +0x13 | Boot voice prompt | H |
| +0x14 | Show boot text | H |
| +0x15 | u16 boot text colour (RGB565) | H |
| +0x17 / +0x19 | u16 boot text y / x | H |
| +0x1B | Show boot image with a 3 s hold | M |
| +0x2C | 32-B boot text | H |
| +0x5C | Voice prompt on | H |
| +0x5D | Key beep on | H |
| +0x5F / +0x62 / +0x65 | Timer settings, converted by `FUN_08014b90` (light timer, menu exit, lock timer) | M |
| +0x61 | Brightness 0..4 (DAC1 level) | H |
| +0x63 | Multi-standby / TX priority (enum 0..3) | L |
| +0x67 | Area display mode | M |
| +0x68 | Alarm type: 0 local siren, 1 transmit tone, 2 both (`FUN_08023b80`) | H |
| +0x6A / +0x6F | u32 APO / AWU timers | M |
| +0x73 | Band range: 0 = "64-999MHz", 1 = "18-64MHz" | H |
| +0x7E | Area-related flag | L |
| +0x80 | PC remote-PTT / TX report enable (see 2.3) | M |
| +0x82 | Enum, 11 values | L |
| +0x83 | Unknown | L |
| +0x8E + 5*i (i < 4) | Band table {u8 mode, u16 low MHz, u16 high MHz}; default 18..1000 | M |
| +0xA3..0xA6 | Scan mode / direction / dwell | M |
| +0xA9 | Unknown | L |
| +0xEA | Unknown | L |
| +0xF1 | SQ level (BK4819 reg write = v*16-0x4FFD) | M |
| +0x100 | u16 single-tone frequency in 0.1 Hz (used for the TX alarm tone) | M |
| +0x102 | MIC gain index | M |
| +0x105 | TX power-related setting | L |
| +0x106 | SQ / glitch threshold | L |
| +0x10C..0x116 | VOX / tone / repeater items | L |
| +0x114 | SPK gain 0..10 | M |
| +0x200..0x209 | DTMF: delay (x100 ms), duration, interval, mode, select, display, TX gain (+0x206/+0x207 go to BK4819 regs 0x70/0x24), control | M |
| +0x20A + 16*n | DTMF code strings | M |
| +0x34A | Unknown | L |
| +0x34C / +0x350 | u32 scan start / end frequency (10 Hz units) | H |
| +0x354 | Unknown | L |

Radio state (0x20003b8a, 0x38 B; defaults at 0x0802B604) (M):
- +0x01 active area (0..2)
- +0x12 index into the 0xF0000 table
- +0x14 + area: VFO/channel mode
- +0x1D + 2*area: u16 channel number
- +0x23: "MPTT" flag
- +0x08: u32 frequency (default 435.125 MHz)

---

## 2. PC programming (CPS) serial protocol

### 2.1 Link (H)
| Item | Value | Evidence |
|---|---|---|
| Port | **USART6** (0x40011400) on **PA11 / PA12 AF8** (TX = PA11, RX = PA12 by the AT32F435 AF map) | `FUN_08012514` |
| Framing | **115200 8N1** | `FUN_08015158`: `FUN_08026bd0(0x1C200)` |
| Alternate baud | Boot key code 0xF sets PA13 high for 500 ms and opens USART6 at **256000** (purpose unknown, M) | `FUN_080035b4` |
| RX | IRQ `vec_87` fills a 4 KB ring at 0x2000742a | |
| TX | 4 KB ring at 0x2000842a, drained one byte per main-loop pass | `FUN_08025f64` |
| Parser | `FUN_08025534`, called from the main loop `FUN_08027cc8` | |
| Checksum | **8-bit sum** of all preceding bytes in the frame (`FUN_08012dd0`) | |

### 2.2 Frames, PC to radio (H, `FUN_08025534` and `FUN_0801e9a4`)
| First byte | Length | Meaning |
|---|---|---|
| `'4'` (0x34) | 5: `34 b1 b2 sub sum` | sub 0x10 = enter **"PC Programming"** (reply 0x06). 0x11 / 0x12 = set / clear the 0x1122ABCD flag. 'R' = remote PTT on (6 s watchdog, needs settings+0x80). 'W' = PTT off. 'T' = keep-alive. **0xEE = end session** (backup, restore and reload; reboot UI if settings magic is 0xABCD) |
| `'5'` | 9: `35 b1 b2 11 id0..id3 sum` | Set the radio ID (ID block word 1), saved to 0x3FF400 and 0x350000 ("ID Set OK!") |
| `'R'` (0x52) | 4: `52 hi lo sum` | **Read 1 KB** at SPI address `(hi<<8 or lo)*0x400`. Reply: `52 hi lo <1024 B> sum` |
| Write cmd 0x40 or 0x90..0xA5 | **1028 (0x404)**: `cmd hi lo <1024 B> sum` | Write 1 KB block n = hi<<8 or lo at `base + n*0x400`. When n == 0 the whole region is erased first. Ack 0x06 |
| `'C'` (0x43) | len+7: `43 x sub len data[len] sum 0D 0A` | **Calibration** sub-protocol, handled by `FUN_08003930`; sum covers len+4 bytes. sub: 'C' connect (reply `43 54 43 00 DA 0D 0A`), 'A'/'P'/'F'/'R' = adjust calibration items (`FUN_0802118c`/`080213fc`/`08021310`/`08021624`), 'S'/'E' = save to sectors 0 and 1 (`FUN_08020770`; reply "CTE"), 'T' = stop TX. Other replies are `43 54 06 00 9D 0D 0A` (OK) or `.. FF .. 96 ..` |

Write command regions (`FUN_0801ce1c`):

| Cmd | Base | Sectors | Content |
|---|---|---|---|
| 0x40 | 0x000000 | 1 | Calibration (flag=1) |
| 0x90 | 0x002000 | 1 | Settings |
| 0x91 | 0x004000 | 12 | Channels |
| 0x92 | 0x01C000 | 1 | VFOs and state |
| 0x93 | 0x01E000 | 32 | Zones |
| 0x94 | 0x05E000 | 52 | Unused by this firmware |
| 0x95 | 0x0C6000 | 5 | Unused |
| 0x96 | 0x0D0000 | 3 | Unused |
| 0x97 | 0x0D6000 | 1 | Unused |
| 0x98 | 0x0F0000 | 1 | 0xF0000 table |
| 0x99 | 0x0FE000 | 1 | BT, GPS and APRS settings |
| 0x9A | 0x100000 | 38 | Boot logo |
| 0x9C | 0x14C000 | 24 | Unused |
| 0x9D | 0x164000 | 36 | Unused |
| 0x9E | 0x188000 | 16 | IME table |
| 0x9F | 0x198000 | 4 | IME table |
| 0xA0 | 0x19C000 | 2 | ASCII font |
| 0xA1 | 0x19E000 | 421 | GBK font |
| 0xA3 | 0x352000 | 158 | Voice prompts |
| 0xA5 | 0x3F0000 | 16 | |
| 0x9B, 0xA2 | 0 | no erase | Raw write at n*0x400 |
| 0xA4 | 0 | erases sector n>>2 when n%4 == 0 | Raw full-chip write; refused (echo 0xA4) unless the flash ID is 0x17, 0x18 or 0x19 |

Flags set per command:
- 0x200011c6: 1 = calibration region, 2 = configuration region, 0 = resources. It decides whether backup runs at 0xEE.
- 0x200011c5 = "something was written".

### 2.3 Radio to PC, unsolicited (H)
- While transmitting with settings+0x80 enabled: `34 52 05 54 DF` every 2 s (`FUN_080257e4`).
- With boot key code 0xE: every GPS NMEA byte is echoed to USART6 (`vec_54` gated by 0x200010da), i.e. GPS pass-through to the PC.

### 2.4 Internal flash (H)
- The app image contains **no FLASH unlock keys** (0x45670123 / 0xCDEF89AB are absent) and no erase/program code.
- The only FLASH controller access is `FUN_0802304c`: FLASH+0x00 (PSR) = 0x154 (wait states / prefetch).
- The app never writes internal flash at runtime. All persistent data lives in SPI flash.
- No reference to 0x0803F800 was found. Firmware update and the last-2 KB page are presumably handled by the bootloader at 0x08000000-0x080027FF (not in this image).
- The app reads internal flash only for defaults: 0x0802B204 settings, 0x0802B604 state, 0x0802B63C VFOs, 0x0802B7B8 BT, 0x0802B7DF GPS, 0x0802B928 APRS, 0x0802AA04 calibration, plus the voice-prompt index at 0x080314F1.
- The vector table is relocated to 0x08002800 in `FUN_08027cc8`.

---

## 3. Other UARTs

### 3.1 GPS: USART2 (0x40004400), receive only (H)
- RX pin PA3 AF7. No TX pin is muxed; PA2 is used for AFSK instead.
- Baud comes from GPS settings [1] via `FUN_08013ce0`: 0 = 4800, **1 = 9600 (default)**, 2 = 14400, 3 = 19200, 4 = 38400, 5 = 56000, 6 = 57600, 7 = 115200, 8 = 128000, 9 = 256000. The port is configured by `FUN_08026b94`.
- **GPS power: PB9 and PB0** are set when GPS is on and cleared (with USART2 disabled) when off (`FUN_08012f50`) (M/H).
- RX ISR `vec_54` builds an NMEA line:
  - The line must start with '$' and is capped at 99 chars.
  - When '*' appears at len-3, it verifies the checksum (`FUN_0801c25c`).
  - Then it dispatches **$xxRMC**, **$xxGGA** (`FUN_08012900`) and **$xxTXT**. If the TXT body contains "OK", the GPS status icon turns green (0x7E0), otherwise red (antenna status) (M).
- "Auto Record", "Record Timer", "Manual record", "Record Track" and "Query Track" are **GPS track logging** to SPI 0xF2000. They are not voice recording (H).

### 3.2 Bluetooth (H for "stub")
- Menu items exist: "Bluetooth On-off", "Bluetooth Mode", "BT MIC Gain", "BT SPK Gain", "BT Device List", "BT Connect Clear", "Station 1/2". Settings live at 0xFE000 (default name "UV100").
- `FUN_08012f50` forces BT on/off = 0 at every boot and disables **UART4** (0x40004C00) without ever configuring it.
- There are **no AT-command strings** and no UART4 pin mux in this image.
- Conclusion: the BT module is not driven by this firmware build.

### 3.3 USART1 / USART3
Referenced only inside the generic library baud function `FUN_08028800` (H). Not used.

---

## 4. Audio

| Function | Implementation | Evidence | Conf |
|---|---|---|---|
| **Beeps / key tones / alarm siren** | **TMR13 CH1 PWM on PA6 (AF9)**. Period 255, 50 % duty, prescaler = 187500/f - 1, giving f Hz with a 48 MHz timer clock. Off with `FUN_08022d00` | `FUN_08005660(freq)`, `FUN_08019168`, `FUN_08021eb0` (800 Hz); siren sweeps 850-1450 Hz in 80 Hz steps and toggles PA14 (`FUN_08023b80`) | H |
| **Voice prompts** | The clip is read from SPI 0x352000+ into RAM 0x20003f16. **TMR2** (PR 499, DIV 11 = **8 kHz**) IRQ `vec_44` writes each unsigned 8-bit sample to TMR13 C1DT. PWM carrier is about 93.75 kHz (DIV 1). Leading 0x80 (silence) samples are skipped | `FUN_08021fdc`, `FUN_08022cb0`, `FUN_080230fc`, `FUN_080233a0`, queue `FUN_080258ec` | H |
| **Speaker amplifier enable** | **PB2**. Set when any audio request bit is set in 0x200011a6 (`FUN_08021e30` sets bit, `FUN_080191dc` drives PB2 high); cleared by `FUN_0802064c` when the last request is released. Request bits: 1 voice, 2 beep, 4 RX audio, 8 tone | | M/H |
| **DTMF TX** | **BK4819 dual-tone generator** (regs 0x71/0x72, e.g. 0x1C1C = 697 Hz, 0x30C2 = 1209 Hz), not the MCU | `FUN_08003f5c`, `FUN_08004118` | H |
| **CTCSS / DCS** | Generated and detected by BK4819 (regs 0x51, 0x07, 0x08) | `FUN_08003d30` | H |
| BK4819 FSK modem (reg 0x58 = 0x37C3, 0x72 = 0x3065 = 1200 Bd, 0x5C/0x5D) | Used for DTMF remote kill/stun data, not APRS | `FUN_08004474`, `FUN_080043e4` | M |
| **APRS AFSK TX** | Software AFSK in the **TMR3** IRQ (`vec_45`, PR 454, DIV 3, about 26.37 kHz): 22 ticks per bit (about 1199 Bd); a GPIO is toggled every 11 ticks (about 1199 Hz) or 6 ticks (about 2198 Hz). Output pin is **PA2** (hw variant PB8 = 1) or **PF1** (variant 0); PA0/PF1 is used as an enable. Bytes are bit-reversed LSB-first (`FUN_0800d530`); 0x7E flags are sent during TX delay | `FUN_08023664`, `FUN_08020628`, `FUN_08020604`, `FUN_080205e0`, `FUN_08003374` | H (timing M) |
| **APRS RX** | In the same TMR3 IRQ (mode 0), **PA2 is sampled as a digital level**. Run length is converted to bit count ((n+10)/21), shifted into bytes and stored in a 1 KB ring at 0x2000a464. Implies an external hardware AFSK demodulator/limiter feeding PA2 (source unknown). HDLC / AX.25 decode follows, with **FCS = CRC-16/X.25** (poly 0x8408, init 0xFFFF, inverted, little-endian) in `FUN_08011374` ("FCS Check: Success/Fail") | `FUN_08025738`, `FUN_08012cdc` | H (demod source L) |
| **DAC1 (PA4)** | 8-bit level from settings +0x61 via table {0x3C, 0x41, 0x46, 0x4C, 0xFF}; probably the **LCD backlight / brightness** drive | `FUN_08021e04`, `FUN_0800830c` | M |
| **DAC2 (PA5)** | Written only during TX when the channel modulation is AM (calibration byte 0x2000081a, cleared on RX); likely the **AM modulation / PA bias level** ("AM Gain") | `FUN_08020f24`, `FUN_0800507c`, `FUN_08004c64` | M |
| **ADC1** | Software-triggered scan of ch7 (PA7) and ch12 (PC2) via DMA1 ch1 into RAM 0x20001070. ch7 is the battery: V = raw*3/51+2. ADC1 is **not** used for audio sampling | `FUN_08002d64`, `FUN_0800836c`, `FUN_08012bcc`, `vec_34` | H |
| Voice recording | **None.** No ADC audio path and no SPI recording area | | H |

TMR1 (PR 999, DIV 47) gives a 1 kHz system tick in `vec_41` (M).

---

## 5. Key take-aways for a replacement firmware
1. **SPI flash** is a W25Qxx on SPI2 (PB12-PB15) with 4 KB sectors. Use the map in 1.3 to stay compatible with the stock CPS image.
2. **Channels**: 1024 x 48 B at 0x4000. Frequencies are u32 LE in 10 Hz units at +5 (RX) and +9 (TX). Tones are u16 at +0x0D (RX) and +0x0F (TX), with type in the top nibble. Name is 16 B at +0x20.
3. **CPS**: USART6 PA11/PA12, 115200 8N1, sum8 checksum:
   - `34 .. .. 10 cs` enters programming mode.
   - `52 hi lo cs` reads 1 KB.
   - `<0x90-0xA5> hi lo 1024B cs` writes 1 KB (erases the region when block 0 is written).
   - `34 .. .. EE cs` ends the session.
4. **Calibration** is at SPI 0x0000 with a copy at 0x1000 (magic 0x9A); factory defaults are in internal flash at 0x0802AA04. **Back up SPI sectors 0-1 before any experiment.**
5. **Audio**: PB2 is the speaker amp enable. Beeps and voice prompts use TMR13 PWM on PA6. DTMF and CTCSS/DCS are handled by BK4819. APRS uses bit-banged AFSK on PA2/PF1 with TMR3 timing.
