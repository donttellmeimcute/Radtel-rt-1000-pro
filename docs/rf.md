# RT-1000 Pro stock firmware: RF / radio-chip subsystem

Source: `out/dec_named.c` (Ghidra), `out/fw.dis`, `out/fw.bin` (base 0x08002800).
GPIO notation: "set" = write to port+0x18 (scr), "clr" = write to port+0x28 (clr).
Confidence: **H** = read directly from code/constants, **M** = strong inference, **L** = guess.

---

## 1. Main transceiver: Beken BK4819 (confidence H)

The register usage matches the BK4819 driver in the Quansheng UV-K5 almost register for register:
- AGC table 0x10–0x14/0x49/0x7B
- REG_47 = 0x6040 | AF<<8
- REG_43 bandwidth 0x3028/0x4048
- REG_51/07/08 CTCSS/DCS
- REG_71 scrambler 0x68DC + n*1032
- REG_36 PA, REG_67 RSSI, REG_65 noise, REG_0D/0E freq-counter
- FSK setup 0x58/0x5C/0x5D/0x72
- DTMF coefficient table in REG_09

### 1.1 Bus (bit-banged 3-wire), GPIOC

| Signal | Pin | Evidence |
|---|---|---|
| SCN (chip select, active low) | **PC8** (0x100) | FUN_0801fc08 clr 0x100 at start, set at end |
| SCK | **PC9** (0x200) | FUN_0801ed1c / FUN_0801ec70 |
| SDA (bidirectional) | **PC7** (0x80) | written in ed1c; read via `gpio_input_data_bit_read(GPIOC,0x80)` in ec70; direction switched by FUN_0801eccc (0 = output PP, 1 = input) |

Functions (H):
- `FUN_0801ed1c(byte)` sends 8 bits MSB first. For each bit: SCK low → SDA = bit → delay(10) → SCK high → delay(10). Data is latched on the **rising edge**.
- `FUN_0801fc08(reg, u16 val)` **BK4819_WriteRegister**: SCK low, SCN low, then sends reg, val>>8 and val&0xFF, then SCN high.
- `FUN_0801fbcc(reg)` **BK4819_ReadRegister**: SCK low, SCN low, sends `reg|0x80`, delay, then `FUN_0801ec70` (SDA set to input; 16 clocks: SCK high → sample PC7 → delay → SCK low → delay, MSB first), then SCN high and SDA back to output.
- The delay is `thunk_FUN_080275a0(10)` (probably a µs delay).

### 1.2 Init sequence: FUN_08004790 (called from main init FUN_080035b4) (H; order verified in disassembly)

```
00=8000            ; soft reset
   delay 10 ms
00=0000
37=1D0F
13=03BE  12=037B  11=027B  10=007A  14=0019  49=2A38  7B=8420   ; FUN_08003b88 (AGC gain table, same as UV-K5)
33=0000  34=0000  35=0000
1E=4C58
1F=A656
3E=A037
7D=E950
26=13A0
75=<tbl A[cal_e2[0x0B]]>            ; FUN_08003a98(0,1,..)
54=<tbl B1[cal_e2[0x0A]]> 55=<tbl B2[..]>   ; FUN_08003a98(0,0,..)
74=<tbl A'[cal_e2[0x09]]>           ; FUN_08003a98(1,1,..)
44=<tbl B1'[cal_e2[0x08]]> 45=<tbl B2'[..]> ; FUN_08003a98(1,0,..)
37=1F0F  (delay 10 ms)  30=0200  30=BFF1     ; FUN_08004990 = "RX on"
71=0000                             ; FUN_0800526e(0)
71=68DC ; 31 &= ~0x0002             ; FUN_08004bcc(0) scrambler off
47=6040                             ; AF mute
read 19, 7E, 2B -> saved to RAM 0x20000838/83A/83C (restored after AM TX)
```

FUN_08003a98 filter tables (cal_e2 = RAM 0x200030e2, an index loaded from calibration):

| idx | 0x54 / 0x44 (`param_2=0`) | 0x55 / 0x45 | 0x75 / 0x74 (`param_2=1`) |
|---|---|---|---|
| 0 | 8BD7 | 3507 | E61C |
| 1 | 8AED | 35E1 | DF22 |
| 2 | 8A96 | 3606 | D42D |
| 3 | 8908 | 3779 | CC35 |
| 4 | 8DDE | 335A | FA02 |
| 5 | 8EB3 | 32BA | F50B (default) |
| 6 | 8F88 | 3201 | F50B |
| default | 8BE3 | 352D | F50B |

When `param_1=0` the function writes 0x54/0x55/0x75; when `param_1=1` it writes 0x44/0x45/0x74.

### 1.3 Set frequency (H)

`FUN_08004530(f)` with f in **10 Hz units**:
```
FUN_0801367c(f)                 ; band select + load per-band cal + set RF path ids
r = f + cal_u16[0] - 0x8000     ; cal_u16[0] = first LE u16 of the band cal block at RAM 0x20003102 (0x8000 = 0 offset)
38 = r & 0xFFFF
39 = r >> 16
```
The same pattern is used in the calibration routines FUN_08004c64, FUN_08004cf0 and FUN_08004db8. The PLL/LO has no band-dependent divider tweak; the band only affects calibration data and the external path switching.

Band table from `FUN_0801367c` (thresholds in 10 Hz units). A sub-band index `(f-start)/5 MHz` (clamped 0..15) selects per-5-MHz calibration bytes:

| band# | range MHz | RX path id (0x20000837) | TX path id (0x20000836) |
|---|---|---|---|
| 0 | 136–174 | 3 | 3 |
| 1 | 400–480 | 4 | 4 |
| 2 | 18–64 | 1 | 1 |
| 3 | 64–136 | 3 | 3 |
| 4 | 174–240 | 3 | 3 |
| 5 | 240–320 | 6 | 3 if <260, else 4 |
| 6 | 320–400 | 4 | 4 |
| 7 | 480–560 | 4 | 4 |
| 8 | 560–660 | 4 | 4 |
| 9 | 840–920 | 5 | 5 |
| 10–14 | 920–1000–1080–1160–1240–1300 | 5 | 5 |

There is no coverage between 660 and 840 MHz, which fits the BK4819's native ranges.

Band calibration block (0x70 bytes): read from SPI flash at `0x20 + band*0x70` (bank 1: `+0x1020`), or from internal-flash defaults at `0x0802aa04 + 0x20 + band*0x70`. RAM copy is at 0x20003102.
- +0 u16: frequency offset (applied as `+cal-0x8000`)
- +3 u16: REG_40 (TX deviation) for wide; +10 u16 for narrow (FUN_0800507c)
- +0x10[16]: REG_36 PA bias (power level A) → RAM 0x20000814
- +0x20[16]: REG_36 PA bias (power level B) → 0x20000815
- +0x30[16]: noise-squelch base, wide → 0x816 (REG_4F)
- +0x50[16]: noise-squelch base, narrow → 0x817
- +0x40[16]: RSSI-squelch base, wide → 0x818 (REG_78)
- +0x60[16]: RSSI-squelch base, narrow → 0x819
- Bands 2 (18–64) and 3 (64–136) also use 16-entry tables at flash 0x6b0/0x6c0 or 0x6d0/0x6e0 (1 MHz or 2 MHz steps). These give 0x2000081b (REG_36 for AM TX) and 0x2000081a (DAC2 value for AM TX).

### 1.4 External RF path switching (H for the mechanism, L for the physical meaning)

The BK4819 GPIO outputs are driven through REG_33 low byte, using read-modify-write.

**RX path**: `FUN_080049e0`. It clears bits 0,1,4,5, then sets according to the RX path id (tbb jump table decoded from fw.dis):

| RX id | REG_33 bit set | Bands |
|---|---|---|
| 0 | none | used only by scan/freq-counter modes |
| 1, 2 | GPIO0 (0x01) | 18–64 MHz |
| 3 | GPIO4 (0x10) | 64–240 MHz (VHF) |
| 4 | GPIO5 (0x20) | 320–660 MHz (UHF) |
| 5, 6 | GPIO1 (0x02) | 240–320 and 840–1300 MHz |

`FUN_080049b8` clears all four RX bits (used before TX).

**TX path**: `FUN_08004ff4`. It first clears MCU **PC15** and REG_33 bits 2,3, then:

| TX id | Action | Bands |
|---|---|---|
| 1, 2 | set **PC15** (MCU pin) | 18–64 MHz (HF/6 m PA path) |
| 3 | REG_33 GPIO3 (0x08) | VHF 64–260 |
| 4, 5, 6 | REG_33 GPIO2 (0x04) | UHF 260–660, 840+ |

`FUN_08004fc0` turns the TX path off: clears PC15 and REG_33 bits 2,3.

**MCU PF0**: cleared when the path id is 3 or 4 (VHF/UHF), set otherwise. This is done for the RX id in FUN_08004594, for the TX id in FUN_0800507c, and on broadcast-radio exit when f≤64 MHz in FUN_0801101c. Its role is probably an HF/1.2G vs V/UHF front-end switch (M/L).

**MCU PB4 and PB1** (FUN_08004594):
- Modulation FM: both cleared.
- AM/SSB: both set.

Other observations:
- AM TX calibration (FUN_08004c64) sets PB4 and clears PB1.
- TX (FUN_0800507c) clears PB1.
- Squelch-open (FUN_0800d08c) clears PB4.

Purpose is likely AM/FM audio or IF-path selection (L).

### 1.5 RX sequence: FUN_08004594 (called via FUN_08021110 / FUN_0802197c) (H)

```
[if coming from TX (state 0x200010d0==3)]
  clr PA14
  tail-tone elimination FUN_080051f0
  TX path off (clr PC15, REG_33 &~0x0C)
  36=007F (PA off)
  delay 20
  restore 19/7E/2B
  if AM: DAC2=0
[if state!=0]
  AGC table (13/12/11/10/14/49/7B)
  47 = 6040 (mute)
set RX freq (38/39) + band select
PF0 per RX path
CTCSS/DCS RX: FUN_08003d30(type, code, ...) or FUN_08003efc(raw code)
4D = A000 + glitch[GlitchTH]
4E = 6F00 + glitch[GlitchTH]                         ; FUN_08004574
4F = ((nsq+2+T1[SQ])<<8) | (nsq-4+T1[SQ])             ; FUN_080048bc, nsq = cal 0x816 (W) / 0x817 (N)
78 = ((rsq+2+T2[SQ])<<8) | (rsq-4+T2[SQ])             ; FUN_08004950, rsq = 0x818 / 0x819
PB4/PB1 by modulation
73: bit4 cleared for FM/AM (AFC on), set for SSB (AFC off)   ; FUN_08004c28
37=1F0F, 30=0200, 30=BFF1                             ; RX enable
REG_33 RX path bits
```

Before this, FUN_08021110 does:
- 43 = 3028 (wide) / 4048 (narrow), selected by channel byte4 bits 7:6
- FUN_08003ec0: `31 &= ~0x08; 28=6B38; 29=AB40` (compander off)
- FUN_08005364: wake from sleep

After it, FUN_0802197c writes 47=6040 and VOX (`31` bit2 = setting 0x10d, FUN_0800532c).

Squelch open (FUN_0800d08c / FUN_08004a68):
- 48 = B00F + vol<<4 (AF gain)
- 75 filter
- 47 = 6140 (FM) / 6740 (AM) / 6540 (SSB: AF type 5, "baseband")
- set PA13, clr PB4
- audio-source flag 4, which enables the speaker amp on PB2

Squelch close (FUN_08010f90): 47 = 6040, clr PA13 (unless setting 0x354 is on).

Squelch tables (H, flash 0x08028a55 / 0x08028a60 / 0x08028a6b, indexed 0..10):
- Glitch TH (setting 0x114) → 4D/4E: `1E 32 46 5A 6E 82 96 AA BE D2 FF`
- SQ level (setting 0x102) → 4F add: `0A 0A 09 08 07 06 05 04 03 02 01`
- SQ level → 78 add: `00 00 02 04 06 0A 10 16 1C 22 26`
- SQ level 0 means squelch is always open (FUN_08004b98 reads 0x0C bit1 otherwise).

There is an extra noise-squelch check in the main loop (around line 4750 of dec_named.c): REG_65 & 0x7F (FUN_080048a0) is compared with 0x816/0x817 using 10-count hysteresis to mute or unmute (REG_47).

### 1.6 TX sequence (H)

`FUN_0800b418` (PTT) clears PA13 and sets PA14, then calls `FUN_0800507c`:
```
REG_33 &= ~0x33 (RX paths off)
47=6040
set TX freq 38/39
PF0 per TX path
CTCSS/DCS TX (FUN_08003d30 / FUN_08003efc)
31 bit1=0, 71=68DC (scrambler off)
7D = E940 + micgain[setting 0x105]                 ; FUN_08004880
40 = cal deviation (wide: cal+3, narrow: cal+10)
FUN_08004e84(tone_active, modulation):
   37=1D0F, delay 10, 52=028F, 30=0200
   FM, no tone:   30=C1FE
   FM with DTMF/tone: 30=C3FA
   AM TX (mod==1):
       48=B003+cal[0xF1]<<4
       47=6140
       7D=E940
       19|=0x8000
       7E&=0x0FC0
       2B: hi byte bits 10,9 cleared, bit8 = (wide?1:0); lo byte |= 0x07
       30=C2FE
REG_33 TX path (PC15 / GPIO3 / GPIO2)
clr PB1
AM:  DAC2(PA5, 8-bit, 0x4000741C) = cal 0x2000081a ; 36 = f(cal 0x2000081b)
else 36 = f(power: chan byte2 bits 7:6 ==1 ? 0x20000814 : 0x20000815)
```

REG_36 encoding (FUN_080048fc, H): `36 = ((p & 0x3F) << 10) | (p ? 0xFF : 0x7F)`. The 6-bit PA bias sits in bits 15:10; bit7 = PA enable; gain bits 6:0 = 0x7F.

There is no MCU DAC power control for FM. **DAC channel 2 (PA5)** is used only for AM TX (FUN_08020f24), which suggests it drives the PA supply or bias for AM (M). DAC1 (PA4) holds backlight levels {3C,41,46,4C,FF} (FUN_08021e04, M).

TX end: see the RX sequence above (the state==3 branch). Sleep/off (FUN_08004e60): clear RX paths, 30=0000, 37=1D00.

### 1.7 CTCSS / DCS (H)

`FUN_08003d30(type, code, ...)`:
- type 0: `51=0000`
- type 1 (CTCSS, code in 0.1 Hz): `51=9040`, `07 = (code*0x19D/200) & 0x1FFF` (×2.065, i.e. Hz×20.65)
- type 2 / 3 (DCS normal / inverted): `w = FUN_08009c54(code, inv)` (23-bit Golay word), then:
  - `51 = 802A` (type 2) / `A02A` (type 3)
  - `07=0AD7` (134.4 Hz)
  - `08 = w & 0xFFF`
  - `08 = 0x8000 + (w>>12)`

`FUN_08003efc(raw24)` writes a raw DCS word: `51 = 802A` with bit11 set when bit24 is set, then 07/08 as above.

Tail-tone elimination (`FUN_080051f0`):
- `51=9040`, `07=046F` (55 Hz)
- or a phase shift: `52 = A28F / C28F / E28F / 823F`

TX setup restores `52=028F`. FUN_080248b8 toggles DCS code ^0x83 during TX (DCS turn-off code).

CTCSS/DCS scan (FUN_08003d18 + FUN_08003bf0):
- `51=0300`, delay 200, RX on
- poll REG_69/6A (DCS 24-bit, bit14 = inverted) and REG_68 (CTCSS: `Hz×10 = (r68&0x1FFF)*200/0x19D`)

### 1.8 RSSI, noise and other reads (H)

- RSSI: `FUN_08004930` reads `REG_67 & 0x1FF`. Display value is `dBm = -(0xA7 - (r67>>1))`, i.e. dBm = r/2 - 167.
- AM/SSB software AGC (FUN_08026704): drives REG_13 from a 97-entry table at **0x080316F4**. Each entry is 3 bytes: u16 REG_13 value, s8 gain dB. It starts `03BE/-7, 009E/0, 00A0/8 ...` and ends `03FF/0`. Displayed dBm is corrected by the gain.
- Noise: REG_65 & 0x7F (FUN_080048a0).
- REG_63 low byte (FUN_0800455c): shown on the debug screen.
- VOX amplitude: REG_64 (FUN_080052f0).
- Interrupt/status: REG_0C (FUN_08003dec, FUN_08004b98).
- Frequency counter ("Freq scan", FUN_08004518 / FUN_080044cc / FUN_08024bac):
  - start with `32=0B01`
  - result `((0D&0x7FF)<<16 | 0E) + 0x8028 - cal`
  - stop with `32=0000`

### 1.9 Tones / DTMF / FSK (H)

- DTMF TX tone pairs, FUN_08003f5c (`71`, `72`). Values are Hz×10.32444:

  | key | 71 | 72 |
  |---|---|---|
  | 0 | 25F3 | 35E1 |
  | 1 | 1C1C | 30C2 |
  | 2 | 1C1C | 35E1 |
  | 3 | 1C1C | 3B91 |
  | 4 | 1F0E | 30C2 |
  | 5 | 1F0E | 35E1 |
  | 6 | 1F0E | 3B91 |
  | 7 | 225C | 30C2 |
  | 8 | 225C | 35E1 |
  | 9 | 225C | 3B91 |
  | A | 1C1C | 41DC |
  | B | 1F0E | 41DC |
  | C | 225C | 41DC |
  | D | 25F3 | 41DC |
  | * | 25F3 | 30C2 |
  | # | 25F3 | 3B91 |

- DTMF RX enable, FUN_08004118:
  - `09 = 006F, 106B, 2067, 3062, 4050, 5047, 603A, 702C, 8041, 9037, A025, B017, C0E4, D0CB, E0B5, F09F`
  - `21=06D8`
  - `24 = 807E | (thr<<7)` (thr from setting 0x207)
  - `70 = v*0x101`, where v = setting 0x206 + 0x80
- DTMF off (FUN_08004104): `24=0`, `70=0`.
- Tone TX (FUN_08005280):
  - 70 high byte 0xC0 | en
  - 30 = BFF1 / C1FE, or 0302 + AF 2, or C3FA + AF 3
  - DTMF TX also sets `48=B32A`
- FSK / MDC1200:
  - setup FUN_08004474(1): `70=00E0 72=3065 58=37C3 5C=5665 5D=0F00` (off: `70=0 58=0`)
  - RX arm FUN_08004368: `3F=2800 59=4028 59=3028`
  - TX FUN_080043e4: `3F=8000 59=8028 59=0028`, 8×`5F=word`, `59=2828`, poll `0C` bit0, `02=0`
  - off FUN_0800434c: `3F=0 59=0028 30=0`
  - FSK RX read FUN_0801edc0 uses REG_0B, REG_5F, REG_02.
- Scrambler (FUN_08004bcc): `71 = 68DC + n*0x408`, `31` bit1.
- VOX: `31` bit2.
- Beep or tone: `71 = n*0x67` (FUN_0800526e).

### 1.10 Factory calibration hooks (M)

Command dispatcher FUN_08003930 (command bytes 0x41/0x45/0x46/0x50/0x52/0x53/0x54, probably over USB/serial):
- FUN_08004c64: AM TX test. `38/39`, `51=0`, `7D=E94F`, `40=34E2`, DAC2, REG_36.
- FUN_08004db8: FM TX test. As above, plus `37=1D0F 52=028F 30=0200 30=C1FE`.
- FUN_08004cf0: RX test with `78`.

They write the calibration arrays at RAM 0x20003f16+0x100 / +0x20 / +0x90 / +0x170, and use PF0 directly. FUN_08004c64 and FUN_08004db8 set PA14 and clear PA13.

---

## 2. Second receiver: Silicon Labs Si4732 / Si4735 (confidence H)

### 2.1 Bus and pins

- **Hardware I2C2** (0x40005800): **PB10 = SCL, PB11 = SDA**, AF4, open-drain. Init in FUN_0802000c (called from board init FUN_08015158). Timing 0x60E02E2E is set by FUN_0802797c/FUN_08027966.
- **7-bit address 0x11** (write 0x22, read 0x23), which corresponds to the Si473x SEN pin tied low.
  - Write: `FUN_08020594` → `FUN_08027a5e(h, 0x22, buf, len, 2000)`
  - Read: `FUN_0801ffe4` → `FUN_080279a8(h, 0x23, buf, len, 2000)`
- Helper functions:
  - `FUN_08020ef4(blob)`: copies a length-prefixed command into RAM 0x20009854, sends it, then waits for CTS. CTS wait is `FUN_08026ca0`, which polls status bit7 up to 40 times.
  - `FUN_0801eda0(n)`: reads n response bytes into 0x20009855.
- **PA8** = Si473x enable or RESET (M). It is set before power-up (FUN_0801f600) and cleared after POWER_DOWN (FUN_08012378). The board init (FUN_08012514) leaves it low.
- Audio-source bit 0x10 (FUN_08021e30(0x10)) enables the speaker amp (PB2).
- POWER_UP ARG2 = 0x05 selects analog audio out. ARG1 has XOSCEN set, so a 32.768 kHz crystal is used.

### 2.2 Command blobs (length-prefixed, flash 0x080355F2–0x08035694) (H)

| addr | bytes | meaning |
|---|---|---|
| 080355F2 | 11 | POWER_DOWN |
| 080355F4 | 10 | GET_REV |
| 080355F6 | 12 00 31 03 78 00 | SET_PROPERTY 0x3103 AM_AVC_MAX_GAIN = 0x7800 (FUN_08020324) |
| 080355FD | 12 00 33 02 00 00 | 0x3302 AM_SOFT_MUTE_MAX_ATTEN (FUN_08020244, &0x3F) |
| 08035604 | 12 00 FF 00 00 00 | 0xFF00 = 0 (disable debug, needed for SSB) |
| 0803560B | 12 00 01 01 00 00 | 0x0101 SSB_MODE (FUN_080203bc/FUN_08020360) |
| 08035612 | 12 00 40 00 00 3F | 0x4000 RX_VOLUME = 63 |
| 08035619 | 01 10 05 | POWER_UP FM, XOSC, analog out |
| 0803561D | 20 03 00 00 00 | FM_TUNE_FREQ (freq at [3..4], 10 kHz units) (FUN_080203f4) |
| 08035623 | 21 0C | FM_SEEK_START up+wrap (bit3 cleared for down) (FUN_0801fedc) |
| 08035626 | 22 02 | FM_TUNE_STATUS (FUN_0801ffb4, reads 8 bytes) |
| 08035629 | 23 01 | FM_RSQ_STATUS INTACK. RSSI = resp[4] (FUN_0801ff80) |
| 0803562C | 80 02 | GPIO_CTL GPO1 output enable (FM init) |
| 0803562F | 81 02 | GPIO_SET GPO1 = 1 (FM init; possibly an FM/AM antenna or LNA switch, L) |
| 08035632 | 12 00 14 03 00 06 | FM_SEEK_TUNE_SNR_THRESHOLD = 6 |
| 08035639 | 12 00 14 02 00 0A | FM_SEEK_FREQ_SPACING (FUN_08020494) |
| 08035640 | 12 00 14 00 00 00 | FM_SEEK_BAND_BOTTOM (FUN_080204cc) |
| 08035647 | 12 00 14 01 00 00 | FM_SEEK_BAND_TOP (FUN_0802045c) |
| 0803564E | 01 11 05 | POWER_UP AM/SW/LW, XOSC, analog out |
| 08035652 | 40 41 37 BE 00 01 | AM_TUNE_FREQ template. FUN_08020208 sets ARG1=0x40 and freq in kHz |
| 08035659 | 41 08 00 00 00 00 | AM_SEEK_START up (0 = down) (FUN_0801fdd0) |
| 08035660 | 12 00 34 00 00 00 | AM_SEEK_BAND_BOTTOM |
| 08035667 | 12 00 34 01 00 00 | AM_SEEK_BAND_TOP |
| 0803566E | 12 00 34 04 00 10 | AM_SEEK_TUNE_RSSI_THRESHOLD |
| 08035675 | 42 02 | AM_TUNE_STATUS |
| 08035678 | 43 01 | AM_RSQ_STATUS. RSSI = resp[4] (FUN_0801ff1c) |
| 0803567B | 48 01 00 | AM_AGC_OVERRIDE. FUN_0801fcc4: gain 0 → AGC on, else AGCDIS=1, index = gain-1 ("AM Gain") |
| 0803567F | 12 00 01 00 00 00 | 0x0100 SSB_BFO. FUN_0801fc6c writes the signed BFO in Hz ("BFO") |
| 08035686 | 01 31 05 | POWER_UP AM + **PATCH** + XOSC (start of SSB patch load) |
| 0803568A | 01 1F 05 | POWER_UP FUNC=15 (query library ID), then reads 8 bytes |
| 0803568E | 40 80 37 BE 00 01 | SSB tune template. FUN_08020504 sets ARG1 = 0x80 (USB) or 0x40 (LSB) and freq in kHz |

SSB_MODE values (FUN_08020360, bandwidth menu 0..5). High byte 0x90 = AFCDIS + AVCEN; low byte = SBCUTFLT<<4 | AUDIOBW:

| menu | value |
|---|---|
| 0 | 0x9004 (0.5 kHz) |
| 1 | 0x9005 (1.0 kHz) |
| 2 | 0x9000 (1.2 kHz) |
| 3 | 0x9011 (2.2 kHz) |
| 4 | 0x9012 (3.0 kHz) |
| 5 | 0x9013 (4.0 kHz) |

### 2.3 Mode bring-up (H)

Dispatcher: `FUN_0801f600`, which shows "Radio Error / Exiting" on failure.

**FM** (`FUN_0801fe40(freq)`):
1. wait CTS
2. POWER_UP FM
3. GPIO_CTL, GPIO_SET
4. wait CTS
5. 0xFF00 = 0
6. FM_TUNE_FREQ
7. SSB_MODE write with arg 0
8. AVC 0x7800
9. volume 0x3F

**AM/LW/MW/SW** (`FUN_0801fd10(freq, agc, bw)`):
1. CTS
2. POWER_UP func 15 (library ID)
3. POWER_DOWN
4. POWER_UP AM
5. CTS
6. 0xFF00 = 0
7. volume 3F
8. soft-mute 0
9. AVC 7800
10. AGC override
11. AM_TUNE_FREQ
12. 0x0101 bandwidth
13. AM_TUNE_STATUS

**SSB** (`FUN_08020140(freq, agc, bw, bfo, usb_lsb)`):
1. CTS
2. library ID
3. POWER_DOWN
4. **patch load** (FUN_08020064)
5. CTS
6. SSB_MODE
7. volume
8. soft-mute 0
9. AVC
10. SSB_BFO
11. AGC
12. SSB tune
13. SSB_MODE
14. AM_TUNE_STATUS

### 2.4 SSB patch blob (H)

- **Flash 0x0803181A, length 0x7BB×8 = 15832 bytes (0x3DD8)**, ending at 0x080355F2.
- It is sent as 1979 raw 8-byte I2C writes after `POWER_UP 01 31 05`, with a 1 ms delay each (FUN_08020064).
- The first bytes (`15 00 0F E0 F2 73 76 2F …`) and the size match the well-known Si4735 "full" SSB patch.

---

## 3. Other chips and buses seen (brief)

| Device | Bus / pins | Evidence | Conf |
|---|---|---|---|
| SPI NOR flash (calibration, settings, channels, backups) | SPI2, **CS = PB12**. Commands: 0x90 (ID), 0x05 / 0x15 (status), 0x06 (WREN), 0x02 (page program), 0x03 (read), 0x20 (sector erase), 0x11 (write SR3). 4-byte address if ID byte is 0x18/0x19 | FUN_080280e0, FUN_08028150, FUN_080281b0, FUN_0802824c, FUN_08028374, FUN_080283d8; FUN_080035b4 reads ID into 0x20001158 | H |
| GPS module | USART2 (PA3 AF7 RX). Power/enable on **PB9 + PB0** (set when GPS on). Baud from setting | FUN_08012f50, FUN_08026b94; strings "GPS Baudrate", "GPS On-off" | M |
| PC / programming serial or Bluetooth link | USART6 (PA11/PA12 AF8) at 115200 (FUN_08026bd0(0x1C200)), 256000 in a special boot mode | FUN_08015158, FUN_080035b4; string "Bluetooth" exists, but no AT command strings were found | L |
| Speaker audio amp enable | **PB2** (set when any audio source bit is in 0x200011a6, cleared when none) | FUN_080191dc, FUN_0802064c | M |
| Power hold / latch | **PF8** (set at init, cleared on low-battery shutdown) | FUN_08012514, FUN_08023e0c | M |
| RX/TX indicators | **PA13** (set on squelch open, cleared on close or TX) and **PA14** (set during TX or voice TX, cleared at TX end). SWD pins are repurposed as GPIO in FUN_08019128. Probably the RX (green) and TX (red) LEDs or T/R switching | FUN_0800d08c, FUN_08010f90, FUN_0800b418, FUN_08004594 | M/L |
| DAC1 (PA4) | backlight brightness levels | FUN_08021e04 | M |
| DAC2 (PA5) | AM TX modulator / PA bias | FUN_08020f24 | M |
| Hardware variant strap | **PB8** input read at boot (0x2000108c). It selects alternate pins PA0/PA2 vs PF1 in FUN_0800538c / FUN_080205bc / FUN_080205e0 / FUN_08020604 | FUN_08012514 | H (mechanism) |

No accelerometer, I2C EEPROM or audio codec was found. I2C1/I2C3 only appear in the generic SDK init (FUN_08027b34); only I2C2 is used, for the Si473x.
