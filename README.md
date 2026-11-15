# Radtel RT-1000 Pro: ingenieria inversa y firmware libre

Este repositorio contiene:

1. **Herramientas** para extraer el firmware del actualizador oficial y flashear cualquier imagen
   por el bootloader de fabrica (`tools/`).
2. **Documentacion del hardware** obtenida por ingenieria inversa del firmware original 260611 (`docs/`).
3. **Un firmware libre completo** escrito desde cero en C para el RT-1000 Pro (`firmware/`).

> **Estado:** el firmware libre compila y sus modulos independientes del hardware estan probados en PC
> (modem APRS, NMEA, tonos). **Todavia no se ha probado en un radio real.** Los pines y las secuencias
> de registros salen del firmware original, pero algunos detalles estan marcados como "probable" en
> `docs/`. Antes de usarlo lee la seccion *Seguridad*.

## Hardware (resumen)

| Bloque | Componente | Conexion |
|---|---|---|
| MCU | Artery **AT32F435** (Cortex-M4F), 256 KB flash, reloj efectivo 48 MHz | |
| Transceptor VHF/UHF 18-1300 MHz | Beken **BK4819** | bit-bang SCN=PC8, SCK=PC9, SDA=PC7 |
| Receptor FM/AM/SW/SSB | Silicon Labs **Si4732** | I2C2 PB10/PB11, direccion 0x11, enable PA8 |
| Pantalla 240x320 | **GC9307** (SPI) | SPI3 PB3/PB5, CS PA15, DC PC13, RST PC14, luz DAC1 PA4 |
| Flash externa | W25Qxx 4 MB+ | SPI2 PB13-15, CS PB12 |
| Teclado 4x4 | filas PA9/PF6/PB6/PB7, columnas PC10/PC11/PC12/PD2 | |
| PTT / encoder / laterales | PC4 / PC1-PC0 / escalera ADC en PC2 | |
| GPS | USART2 RX PA3, alimentacion PB9+PB0 | |
| PC / programacion | USART6 PA11/PA12, 115200 | |
| Audio | amplificador PB2, PWM TMR13 en PA6 | |
| APRS | modem por GPIO PA2/PF1 + TMR3 | |
| Alimentacion | retencion PF8, bateria ADC PA7 | |

Detalle completo con evidencias en [`docs/hardware.md`](docs/hardware.md), [`docs/rf.md`](docs/rf.md),
[`docs/display.md`](docs/display.md) y [`docs/storage_comms.md`](docs/storage_comms.md).

## Funciones del firmware libre

- VFO dual (A/B) de 18 a 660 MHz y de 840 a 1300 MHz, modo canal (1024 canales) y zonas (250).
- **Compatible con el CPS oficial:** usa el mismo formato de canales y zonas en la flash SPI y el
  mismo protocolo de programacion por cable.
- Usa la **calibracion de fabrica** del radio (nunca la modifica).
- FM ancho/estrecho, AM (RX/TX), USB/LSB (RX), CTCSS/DCS (normal e invertido), cola de tono STE,
  scrambler, VOX, potencia alta/baja, desplazamiento de repetidor, inversion RX/TX.
- Escaneo de VFO y de canales (tiempo / portadora / parar), doble escucha.
- **Receptor de radiodifusion** con el Si4732: FM 64-108 MHz, MW, LW, SW y **SSB** (requiere instalar
  el parche, ver abajo), con control de ancho de banda, AGC, BFO y busqueda.
- **APRS**: baliza manual o periodica (posicion GPS o fija), recepcion y decodificacion AX.25,
  lista de paquetes y registro en la flash.
- **GPS**: posicion, altitud, velocidad, rumbo, hora (zona horaria), localizador Maidenhead y
  registro de traza en la flash.
- **DTMF**: envio manual y 16 memorias, PTT-ID, decodificacion, kill/stun/wake remoto opcional.
- Busqueda de CTCSS/DCS, frecuencimetro, analizador de espectro.
- Alarma de emergencia (local, TX o ambos), tiempo maximo de TX, bloqueo de teclado, auto-bloqueo,
  pitido de fin de TX, 5 niveles de brillo, apagado de pantalla, funciones configurables en las
  teclas laterales, logo de inicio desde la flash, mensajes de voz.
- Proteccion de TX: por defecto solo transmite en bandas de radioaficionado/PMR/FRS (configurable).

No incluido: Bluetooth. El firmware original tampoco lo usa (es un menu sin funcion; ver
`docs/storage_comms.md` 3.2).

## Uso

### 1. Respalda la flash SPI (calibracion de fabrica)
Con el **firmware original** y el radio encendido normalmente:
```
pip install pyserial
python3 tools/backup_spiflash.py -p /dev/ttyUSB0 -o respaldo.bin          # calibracion, ajustes, canales
python3 tools/backup_spiflash.py -p /dev/ttyUSB0 --full -o completo.bin   # 4 MB completos
```

### 2. Compila
```
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi
cd firmware && make          # genera build/rt1000.bin
make test                    # pruebas en PC
```

### 3. Flashea
Manten PTT y enciende el radio (modo bootloader), luego:
```
python3 tools/flash.py -p /dev/ttyUSB0 firmware/build/rt1000.bin
```

### 4. Instala los recursos con derechos de autor (opcional)
El parche SSB del Si4735 y el indice de los mensajes de voz no se pueden redistribuir. Este script
los extrae de **tu** actualizador oficial y los copia a la flash del radio:
```
python3 tools/install_resources.py -p /dev/ttyUSB0 Radtel_RT-1000Pro_Update_260611_3.exe
```

### Volver al firmware original
```
python3 tools/extract_firmware.py Radtel_RT-1000Pro_Update_260611_3.exe -o stock
python3 tools/flash.py -p /dev/ttyUSB0 stock.bin      # o usa el actualizador oficial
```

## Controles

| Tecla | Pantalla principal |
|---|---|
| 0-9 | entrada directa de frecuencia (8 digitos) o numero de canal |
| Arriba/Abajo, encoder | cambiar frecuencia / canal |
| MENU | menu (largo: lista de canales) |
| EXIT | borrar digito (largo: VFO -> MR -> Zona) |
| `*` | escanear (largo: invertir TX/RX) |
| `#` | cambiar area A/B (largo: bloquear teclado) |
| Laterales | configurables (monitor, escaneo, radio AM/FM, baliza APRS, ...) |

La asignacion fisica de MENU/Arriba/Abajo/EXIT/`*`/`#` sigue los codigos de tecla del firmware
original; si en tu radio no coinciden con los rotulos, ajusta `keymap` en `firmware/src/keys.c`.

## Seguridad

- El bootloader de fabrica no se modifica, por lo que siempre se puede volver al firmware original.
- La calibracion (flash SPI 0x000000-0x001FFF) solo se lee; aun asi **haz el respaldo**.
- Los pines PA13/PA14 (SWD) se usan como LEDs, igual que en el original. Manten **EXIT** al
  encender para dejarlos libres y depurar con SWD.
- Transmitir fuera de las bandas autorizadas es ilegal en la mayoria de paises.

## Reproducir la ingenieria inversa

```
python3 tools/extract_firmware.py Radtel_RT-1000Pro_Update_260611_3.exe -o stock
# Ghidra 11.x headless:
analyzeHeadless proj rt1000 -import stock.bin -loader BinaryLoader -loader-baseAddr 0x08002800 \
   -processor ARM:LE:32:Cortex -scriptPath tools/ghidra -preScript Prep.java -postScript DumpAll.java decomp.c
python3 tools/ghidra/resolve_literals.py stock.bin decomp.c decomp_resuelto.c
```
`tools/ghidra/ildump.py` desensambla el IL del actualizador .NET (requiere `dnfile` y `dncil`).

## Licencia

Codigo propio bajo licencia Apache-2.0 (ver `LICENSE`). Las fuentes de mapa de bits se generan de
DejaVu Sans Mono (licencia libre de Bitstream Vera/DejaVu). Este proyecto no contiene codigo ni datos
del firmware original de Radtel; las herramientas solo trabajan sobre archivos que el usuario ya posee.
