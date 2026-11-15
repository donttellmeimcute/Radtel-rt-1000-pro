#pragma once
/* Mapa de la flash SPI externa. Se respeta el formato del firmware original
 * para que el software de programacion (CPS) oficial siga funcionando y para
 * no tocar la calibracion de fabrica. Ver docs/storage_comms.md.             */
#define FLASH_CAL           0x000000u   /* calibracion de fabrica (magic 0x9A) - SOLO LECTURA */
#define FLASH_CAL_BACKUP    0x001000u
#define FLASH_STOCK_SETTINGS 0x002000u  /* ajustes formato original (se importan) */
#define FLASH_CHANNELS      0x004000u   /* 1024 x 0x30 */
#define FLASH_CHANNELS_BAK  0x010000u
#define CHANNEL_COUNT       1024
#define CHANNEL_SIZE        0x30
#define FLASH_VFOS          0x01C100u   /* 7 x 0x30 */
#define FLASH_ZONES         0x01E000u   /* 250 x 0x208 */
#define ZONE_COUNT          250
#define ZONE_SIZE           0x208
#define FLASH_GPS_TRACK     0x0F2000u   /* u16 BE cuenta + 1000 x 0x30 */
#define GPS_TRACK_MAX       1000
#define FLASH_APRS_STOCK    0x0FE800u   /* ajustes APRS formato original */
#define FLASH_BOOT_LOGO     0x100000u   /* 240x320 RGB565 */
#define FLASH_APRS_LOG      0x12E000u   /* u16 BE cuenta + 400 x 0x100 */
#define APRS_LOG_MAX        400
#define FLASH_FONT_GBK24    0x19E000u
#define FLASH_FONT_ASCII24  0x19C000u
#define FLASH_VOICE         0x352000u
/* Regiones propias del firmware libre (zona no usada por el original ni por el CPS) */
#define FLASH_OS_SETTINGS   0x092000u
#define FLASH_OS_SETTINGS_BAK 0x093000u
#define FLASH_SSB_PATCH     0x094000u   /* "SSBP" + u32 len + datos (lo instala tools/install_ssb_patch.py) */
#define FLASH_VOICE_INDEX   0x09C000u   /* indice de mensajes de voz (lo instala el usuario) */
