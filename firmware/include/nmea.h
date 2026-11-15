#pragma once
#include <stdint.h>
typedef struct {
    uint8_t valid;           /* fix valido */
    int32_t lat_e7, lon_e7;  /* grados * 1e7 (sur/oeste negativos) */
    int32_t alt_dm;          /* altitud en decimetros */
    uint16_t speed_kmh10;    /* velocidad km/h * 10 */
    uint16_t course_deg10;   /* rumbo grados * 10 */
    uint8_t sats;
    uint8_t hh, mm, ss, day, mon, year;
} gps_fix_t;
/* Alimentar byte a byte; devuelve 1 cuando se completo una sentencia valida */
int nmea_feed(gps_fix_t *fix, char c);
