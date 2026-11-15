# RT-1000 Pro stock firmware: board init, pin map, inputs, power

Source: dec_named.c / fw.dis (app image at 0x08002800). Confidence levels: **C** = confirmed (direct code evidence),
**L** = likely (strong circumstantial evidence), **G** = guess.
Helper outputs: `gpio_writes.txt` (generated) lists every GPIO SET/CLR/TOG store found by a register-tracking
disassembly scan (`tools/ghidra/gpioscan.py`). The scan also catches stores that the decompiler hid inside jump tables.

gpio_init struct = `{u32 pins; u8 out_type(0=PP,1=OD); u8 pull(0 none,1 up,2 down); u8 mode(0 in,1 out,2 mux,3 analog); u8 drive(1 stronger,2 moderate)}`.
gpio_default_para_init (0x08027810) = pins 0xFFFF, PP, no pull, input, drive 1.
NOTE: `gpio_input_data_bit_read(port, MASK)` takes a pin **mask**, not a pin number.

## 1. Startup sequence

| Step | Function | What it does | Conf |
|---|---|---|---|
| Reset | Reset_Handler 0x08002ac0 -> SystemInit 0x08022fa8 | Enables the FPU (CPACR \|= 0xF00000), resets CRM to HICK, **sets VTOR = 0x08000000** (the bootloader's table) | C |
| __main | 0x080029e0 | Keil scatter-load, then calls C main | C |
| main | **FUN_08027cc8** | true C `main()` | C |
| 1 | FUN_0802304c | clock config: flash reg 0x40023C00=0x154, PWC LDO (0x40007010 \|=3), LICK on, HICK on, PLL src=HICK ns=96 ms=1 fr=8 (enum 3), AHB/1, APB2/1 (bits13-15=0), APB1/2 (bits10-12=4), auto-step, SCLK=PLL, then system_core_clock_update FUN_08028400 | C |
| 2 | `*0xE000ED08 = 0x08002800` | **VTOR is moved to the app vector table**. SystemInit had pointed it at 0x08000000, so an open firmware must do the same if it keeps the bootloader | C |
| 3 | FUN_080275ac | delay init: SysTick clock = HCLK, fac_us = SystemCoreClock/1e6. SysTick is used **only for polled delays** (FUN_080084d4 = delay_ms, FUN_080275a0 = busy loop). vec_15 (SysTick ISR) is empty | C |
| 4 | delay 200 ms | | C |
| 5 | **FUN_08015158** board init (see below) | | C |
| 6 | FUN_080035b4(0) | app init: checks startup key combos (keypad code 0xF: USART6 re-init at 256000 Bd plus a PA13 blink; 0xE; 0xC toggles an ON/OFF flag at SPI-flash 0x3FF000; 5 toggles magic 0x1122ABCD), loads settings from SPI flash (FUN_08012f50), and so on | C |
| 7 | main loop | `for(;;){ if(flag) FUN_0801f820 (recovery); if(!pcmode){ if(mode==2) poll BK4819 reg 0x0C else FUN_08026cd4 (UI/radio scheduler) } else FUN_08003930 (PC/factory command handler); FUN_08025534 (USART6 RX parser); FUN_08025f64 (USART6 TX pump) }` | C |

Board init FUN_08015158, in order:
1. FUN_0801dfe8: peripheral clock enables. The arguments are AT32 periph IDs (reg_off<<16 \| bit): GPIOA/B/C/D/F, DAC, ADC1, DMA1, SPI2, SPI3, USART2, UART4, USART6, TMR1, TMR2, TMR3, TMR13, I2C2. (C)
2. **FUN_08012514**: main GPIO init (table below). (C)
3. FUN_0800538c: APRS modem pins, which depend on the HW variant strap PB8. (C)
4. FUN_0800836c: DMA1 ch1 (0x40026008) runs from ADC1 data reg 0x4001204C to 0x20001070, 2 half-words, circular. (C)
5. FUN_08002d64: ADC1 with 2-channel ordinary sequence: rank1 = **ch7 (PA7)**, rank2 = **ch12 (PC2)**, scan mode, DMA, calibrated. Software-triggered in FUN_08012bcc. (C)
6. FUN_080113f8: SPI2 plus external SPI NOR flash (CS PB12). Reads JEDEC ID; if capacity byte 0x18/0x19 it sets 4-byte addressing (cmd 0xB7). (C)
7. FUN_08018f80: SPI3 plus LCD pins. (C)
8. FUN_0800830c / FUN_08008328: DAC ch1 and ch2 enabled, DAC2 = 0. (C)
9. FUN_08023170: **TMR1 tick**, started. FUN_0802345c: TMR3 (AFSK) configured but not started. (C)
10. DAC1 (DHR8R1 0x40007410) = 0, so the backlight is off. (C)
11. FUN_08018d68: LCD init (240x320, controller cmds 0xFE/0xEF... = Galaxycore GC9xxx-style, COLMOD 0x05 RGB565, MADCTL 0xE0). (C for sequence, L for controller family)
12. FUN_0802000c: I2C2 (0x40005800) on PB10/PB11. (C)
13. delay 100. FUN_08026bd0: **USART6 (0x40011400) at 115200**. (C)
14. FUN_0801c2aa: NVIC. AIRCR PRIGROUP=3 (4 preempt bits). IRQ18 ADC (pri 0), IRQ11 DMA1_CH1 (0), IRQ71 USART6 (0), IRQ38 USART2 (1), IRQ25 TMR1_OVF (1), IRQ28 TMR2 (0), IRQ29 TMR3 (0). (C)
15. Battery wait: loops (200 ms steps) until battery >= 0x3C (6.0 V). (C)
16. FUN_08019128: **PA13/PA14 become GPIO outputs (low). This disables SWD**, so a debugger needs connect-under-reset. (C)

### Clock and timers
- With the nominal AT32F435 PLL formula, 8 MHz x 96 / (1 x 8) gives 96 MHz. However, the firmware's own `system_core_clock_update` (0x08028400, disasm 0x080284a4..0x080284ae) divides by an **extra /2**, so SystemCoreClock = **48 MHz**. Every timer constant only makes sense with a 48 MHz timer clock: 1 kHz tick, 8 kHz sample rate, 1200 Bd AFSK, and `187500/f` for the TMR13 tone (=48e6/256). **So the effective SYSCLK = 48 MHz, APB2 = 48, APB1 = 24 (timer clock 48).** (L, strongly consistent)
- **Tick = TMR1 (0x40010000)**: DIV=47, PR=999, so the overflow ISR (vec_41, IRQ25) runs at **1 kHz (1 ms)**. The ISR runs:
  - FUN_080191dc: audio-amp delayed enable plus the tone sequencer
  - FUN_08021d58: keypad matrix scan, one row per ms
  - FUN_080071a4: rotary encoder sampling
  - about 20 software countdown timers
  - a ms counter (0x200010e6) that rolls into a seconds counter (0x20001134)

  (C)
- TMR2 (0x40000000): DIV=11, PR=499, giving **8 kHz**. Its ISR (vec_44) feeds 8-bit samples from 0x20003F16 into TMR13 C1DT (0x40001C34) for voice/PCM playback. Started by FUN_08022cb0. (C)
- TMR3 (0x40000400): DIV=3, PR=454, giving 26.37 kHz. Its ISR (vec_45) is the **APRS 1200 Bd AFSK modem**: 22 ticks/bit, tone toggle every 11 ticks (1200 Hz) or 6 ticks (2200 Hz) on TX, NRZI edge timing on PA2 on RX. (C)
- TMR13 (0x40001C00): PWM ch1 on PA6, PR=0xFF.
  - Beep: DIV = 187500/f - 1, 50 % duty (FUN_08005660).
  - PCM: DIV=1 (FUN_080230fc).
  - Alarm siren: 850 to 1450 Hz sweep (FUN_08023b80).

  (C)

## 2. GPIO table (final state after board init)

Init level: H/L = written by SET/CLR at init, "-" = not written. Drive: S=stronger(1), M=moderate(2).

| Pin | Mode | Pull | Drive | Init | AF | Function | Evidence | Conf |
|---|---|---|---|---|---|---|---|---|
| PA0 | out PP (variant0 only) | - | M | L | - | APRS RX demod/modem enable (variant0) | FUN_0800538c, FUN_080205e0/bc, FUN_08025738 | L |
| PA2 | in | up | - | - | - | APRS demod data in (TMR3 ISR samples it). On variant1 it is switched to output for TX AFSK tone (toggled) | vec_45, FUN_08003374/08003340, FUN_08020604/28 | C (role) |
| PA3 | mux | none | S | - | AF7 USART2_RX | **GPS NMEA RX** (USART2 0x40004400, baud from GPS settings) | FUN_08012f50, FUN_0800c8d0, vec_54 | C |
| PA4 | analog | - | S | - | DAC1_OUT | **LCD backlight brightness** (DHR8R1; levels {0x3C,0x41,0x46,0x4C,0xFF} indexed by "Brightness" setting; 0 = off) | FUN_08021e04, FUN_0801f790, FUN_08022eb8 | L |
| PA5 | analog | - | S | - | DAC2_OUT | RF analog control (DHR8R2 0x4000741C). Written in TX setup and in AM mode: TX power/APC or AM mod bias candidate | FUN_08020f24 <- FUN_08004c64/0800507c/08004594 | G (role) |
| PA6 | mux | none | S | - | AF9 TMR13_CH1 | **Beep / voice-prompt PCM / alarm tone PWM** to the audio amp | FUN_08005660, FUN_080230fc, vec_44, FUN_08023b80 | C |
| PA7 | analog | - | - | - | ADC1_IN7 | **Battery voltage**. `batt = raw*3/51 + 2` (stored at 0x20001088, appears to be 0.1 V units). Boot needs >=60, "Please Charge" <61, shutdown <59 (or <40 debounced) | FUN_08012bcc, FUN_08023e0c, FUN_0802471c | C (channel), L (scale) |
| PA8 | out PP | none | M | L | - | **Broadcast/SW receiver (SI47xx on I2C2) power enable**. Set before power-up (+100 ms), cleared after power-down cmd | FUN_0801f600, FUN_08012378 | L |
| PA9 | out PP | none | M | H | - | **Keypad row 0** (active-low scan) | FUN_08021d58 | C |
| PA10 | in | up | - | - | - | unknown. No reader found (no IDT/EXINT use) | - | - |
| PA11 | mux | none | S | - | AF8 USART6_TX | **PC/programming serial TX** (USART6, 115200) | FUN_0801d0e4, FUN_08026bd0 | C (periph), L (AF map) |
| PA12 | mux | none | S | - | AF8 USART6_RX | PC serial RX (vec_87 = USART6 IRQ) | vec_87 | C/L |
| PA13 | out (after FUN_08019128) | none | M | L | (SWDIO) | **Green LED: RX / busy** (set on squelch open, cleared on RX end; blinks in a boot mode) | FUN_0800b25c, FUN_08004a68 path, FUN_080105c0, FUN_08004c64 (clr on TX) | L |
| PA14 | out (after FUN_08019128) | none | M | L | (SWCLK) | **Red LED: TX** (set in all TX paths incl. APRS beacon; toggled by alarm) | FUN_08004c64, FUN_08004db8, FUN_08002e80, FUN_08023b80 | L |
| PA15 | mux | none | S | - | AF6 SPI3_NSS | LCD CS (hardware NSS) | FUN_08018f80 | L |
| PB0 | out PP | none | M | L | - | **GPS power** (switched together with PB9) | FUN_0800c8d0, FUN_08012f50, FUN_0800e494, FUN_08022eb8 | L |
| PB1 | out PP | none | M | **H** | - | RF control candidate: cleared in TX and FM RX, set in AM/SSB RX (with PB4) | FUN_08004594, FUN_08004c64/db8/cf0 | G |
| PB2 | out PP | none | M | L | - | **Speaker audio amplifier enable**. Request bitmask at 0x200011a6 (1=PCM, 4=RX audio, 8=beep, 0x10=broadcast RX); delayed set from 1 ms ISR, cleared when no requester | FUN_08021e30, FUN_0801f7dc, FUN_080191dc, FUN_0802064c | C |
| PB3 | mux | none | S | - | AF6 SPI3_SCK | LCD SCK | FUN_08018f80 | C |
| PB4 | out PP | none | M | L | - | RF control candidate: set in AM/SSB RX and in the 'A' TX test path (FUN_08004c64, which looks like AM TX), cleared in FM | FUN_08004594, FUN_08004c64, FUN_08004a68 | G |
| PB5 | mux | none | S | - | AF6 SPI3_MOSI | LCD MOSI | FUN_08018f80 | C |
| PB6 | out PP | none | M | H | - | **Keypad row 2** | FUN_08021d58 | C |
| PB7 | out PP | none | M | H | - | **Keypad row 3** | FUN_08021d58 | C |
| PB8 | in | up | - | read once | - | **HW variant strap** (flag 0x2000108c). Selects APRS modem pin set (see notes) | FUN_08012514 | C |
| PB9 | out PP | none | M | L | - | **GPS power** (with PB0) | as PB0 | L |
| PB10 | mux OD | up | S | - | AF4 I2C2_SCL | I2C2 to the SI47xx AM/FM/SW receiver | FUN_0802000c, FUN_0801fe40 | C (bus), L (device) |
| PB11 | mux OD | up | S | - | AF4 I2C2_SDA | " | " | " |
| PB12 | out PP | up | S | H | - | SPI flash CS | FUN_080113f8, FUN_08028150 etc. | C |
| PB13/14/15 | mux | up | S | - | AF5 SPI2 SCK/MISO/MOSI | External SPI NOR flash (settings, voice data) | FUN_080113f8 | C |
| PC0 | in | up | - | - | - | **Rotary encoder B** (read with mask 1) | FUN_080071a4 (1 ms ISR) -> FUN_0802517c -> FUN_0801636c | C |
| PC1 | in | up | - | - | - | **Rotary encoder A** (mask 2) | same | C |
| PC2 | analog | - | - | - | ADC1_IN12 | **Side keys resistor ladder**. raw<0x75D -> key 0x17, <0xA9D -> 0x16, <0xDD6 -> 0x18, else none. Setup menu has Side Key 1, Side Key 2 and Alarm Key; exact mapping unknown | FUN_08014ac4, FUN_08007b1c | C (ladder), G (which key) |
| PC3 | out PP | none | M | L | - | Set only while an APRS beacon is transmitted: AFSK-to-mic audio switch candidate | FUN_08023664 | G |
| PC4 | in | up | - | - | - | **PTT (active low)**. Keys 0x16/0x17 can act as PTT for other VFOs when setting (0x20003b8a+0x23) is set | FUN_080241c8, FUN_08007b1c | C |
| PC5 | in | **down** | - | - | - | unknown, no reader found | - | - |
| PC6 | in | up | - | - | - | unknown, no reader found | - | - |
| PC7 | out PP / in | none | M | L | - | **BK4819 SDATA** (bit-bang, turned around via FUN_0801eccc) | FUN_0801ed1c, FUN_0801ec70 | C |
| PC8 | out PP | none | M | L | - | **BK4819 SCN** (CS, active low) | FUN_0801fbcc/FUN_0801fc08 | C |
| PC9 | out PP | none | M | L | - | **BK4819 SCK** | FUN_0801ed1c | C |
| PC10 | in | up | - | - | - | **Keypad col 0** | FUN_08021c58 | C |
| PC11 | in | up | - | - | - | **Keypad col 1** | " | C |
| PC12 | in | up | - | - | - | **Keypad col 2** | " | C |
| PC13 | out PP | none | S | L | - | LCD D/C (low = command) | FUN_080190b4 | C |
| PC14 | out PP | none | S | L | - | LCD RESET (pulsed in FUN_08018d68) | FUN_08018d68 | C |
| PC15 | out PP | none | M | L | - | RF band-select candidate: set for band classes 1/2 (about 18-64 MHz). VHF/UHF selection is done via BK4819 GPIO reg 0x33 bits 2/3 | FUN_08004ff4 (jump-table cases), FUN_08004fc0 | L (band switch) |
| PD2 | in | up | - | - | - | **Keypad col 3** | FUN_08021c58 | C |
| PF0 | out PP | none | M | L | - | RF path switch candidate: low for VHF/UHF classes (3,4) and broadcast-FM mode, high for 18-64 MHz and >=840 MHz | FUN_08004594, FUN_0800507c, FUN_0800bde4, FUN_0801101c | L (RF switch), G (exact) |
| PF1 | out PP (variant0) / input (variant1) | none | M | L | - | variant0: APRS AFSK TX tone output (toggled by TMR3 ISR). Variant1: effectively unused (configured input) | FUN_0800538c, FUN_08020604/28 | L |
| PF6 | out PP | none | M | H | - | **Keypad row 1** | FUN_08021d58 | C |
| PF8 | out PP | none | M | **H** | - | **Power hold / main power enable**. Cleared on low-battery shutdown, followed by an infinite loop or reset | FUN_08023e0c, FUN_0802471c | L |

Peripherals used without any pins configured: UART4 (0x40004C00; clock enabled, then disabled in FUN_08012f50; maybe for Bluetooth on another build) (G).
The AF numbers assume AT32F435 mux tables equal STM32F4 AF numbering for these pins: AF4 I2C, AF5 SPI2, AF6 SPI3, AF7 USART2, AF8 USART6, AF9 TMR13. (L)

## 3. Inputs in detail

**Keypad 4x4 matrix** (FUN_08021d58 in the 1 ms ISR):
- Rows are PA9, PF6, PB6, PB7, driven low one at a time. Columns are PC10, PC11, PC12, PD2 (pull-up, active low).
- FUN_08021c58 builds a 16-bit mask at 0x200010dc with bit = row*4 + col.
- FUN_08013d94 maps the mask to a key code:

| row\col | PC10 | PC11 | PC12 | PD2 |
|---|---|---|---|---|
| PA9 | 1 | 4 | 7 | 0x0E |
| PF6 | 2 | 5 | 8 | 0 |
| PB6 | 3 | 6 | 9 | 0x0F |
| PB7 | 0x0A | 0x0B | 0x0C | 0x0D |

The masks 0x1111 (all rows on col PC10) and 0x8888 (all rows on col PD2) also map to 0x0E and 0x0F. This suggests keys wired straight from a column to GND, or the layout of a different keypad revision. (C for the table, G for the explanation)

**Side keys:** read through the ADC ladder on PC2 (key codes 0x16/0x17/0x18). **PTT** is PC4. The **encoder** is on PC0/PC1, sampled every 1 ms. Key handling and long press (>800 polls) are in FUN_08007b1c. (C)

## 4. Power
- Power hold: **PF8** high at init. It is cleared on battery < 4.0 V (double-checked) or < 5.9 V, after which the radio goes dead (FUN_08023e0c / FUN_0802471c, then FUN_08026e10 = NVIC_SystemReset). (L)
- No power-button input was found. Power on is probably the volume-knob switch, with PF8 holding the regulator.
- "Power Off!" in FUN_08022eb8 / FUN_080239e8 is a **soft sleep**: it turns off the LEDs, GPS (PB9/PB0), APRS modem, backlight DAC, SI47xx, BK4819 RX and TMR3. The firmware then loops until a timer wakes it. (C)
- Battery ADC is PA7 / ADC1_IN7. (C)
- No flashlight GPIO was found, and there are no torch strings in the image. "Carrier LED" is a setting for the green LED. (C that no candidate exists in this image)

## 5. Other notes
- BK4819 is bit-banged on PC7/PC8/PC9 with ~10-iteration busy-loop delays (FUN_0801ed1c write, FUN_0801ec70 read, FUN_0801fbcc reg read, FUN_0801fc08 reg write). (C)
- The USART6 command protocol (main loop FUN_08003930) has commands 'A','C','E','F','P','R','S','T'. 'A' calls FUN_0802118c (TX via FUN_08004c64), 'F'/'P' call FUN_08004db8 (TX), 'R' calls FUN_08004cf0 (RX test), 'T' stops. This looks like the factory/calibration interface. (L)
- APRS hardware variants (PB8 strap):
  - Variant 0: enable = PA0, TX tone = PF1, RX data = PA2.
  - Variant 1: enable = PF1 (but PF1 is configured as input, so the write has no effect), TX tone and RX data both on PA2 (direction switched).
  
  (C)
