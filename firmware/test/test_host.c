/* Pruebas en PC de los modulos independientes del hardware:
 *   gcc -Iinclude -o /tmp/t test/test_host.c src/afsk.c src/nmea.c src/tones.c && /tmp/t */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "afsk.h"
#include "nmea.h"
#include "tones.h"

static int got; static char gotinfo[256];
static void cb(const uint8_t *f, int n)
{
    int i = 0; while (i < n && !(f[i] & 1)) i++;   /* fin de direcciones */
    i += 3;
    memcpy(gotinfo, f + i, (size_t)(n - i)); gotinfo[n - i] = 0; got++;
}

int main(void)
{
    /* AFSK lazo TX->RX */
    uint8_t fr[AX25_MAX_FRAME]; char info[64];
    aprs_position(info, 194326000, -991332000, '/', '>', " RT-1000 libre");
    printf("APRS: %s\n", info);
    int n = ax25_build_ui(fr, "APRT10", "XE1ABC-9", "WIDE1-1,WIDE2-1", info);
    static afsk_tx_t tx; static afsk_rx_t rx;
    afsk_tx_start(&tx, fr, n); afsk_rx_init(&rx, cb);
    int s, cnt = 0, noise = 12345;
    while ((s = afsk_tx_next(&tx)) >= 0) {
        noise = noise * 1103515245 + 12345;
        int16_t v = (int16_t)((s - 128) * 40 + ((noise >> 16) % 600));
        afsk_rx_sample(&rx, v); cnt++;
    }
    for (int i = 0; i < 64; i++) afsk_rx_sample(&rx, 0);
    printf("muestras %d, tramas recibidas %d: '%s'\n", cnt, got, gotinfo);
    assert(got >= 1 && !strcmp(gotinfo, info));

    /* Ruta de nivel digital (modem externo) a la tasa del TMR3 del radio: 26373 Hz, 22 ticks/bit */
    got = 0; gotinfo[0] = 0;
    afsk_tx_start(&tx, fr, n); afsk_rx_init_rate(&rx, cb, 26373);
    int tone;
    while ((tone = afsk_tx_next_tone(&tx)) >= 0)
        for (int k = 0; k < 22; k++) afsk_rx_level(&rx, tone);
    printf("nivel: tramas %d '%s'\n", got, gotinfo);
    assert(got >= 1 && !strcmp(gotinfo, info));

    /* NMEA */
    gps_fix_t f = {0};
    const char *nm = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n"
                     "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    for (const char *p = nm; *p; p++) nmea_feed(&f, *p);
    printf("GPS valid=%d lat=%ld lon=%ld alt=%ld dm sats=%d %02d:%02d:%02d\n", f.valid,
           (long)f.lat_e7, (long)f.lon_e7, (long)f.alt_dm, f.sats, f.hh, f.mm, f.ss);
    assert(f.valid && f.lat_e7 == 481173000 && f.lon_e7 == 115166666 && f.alt_dm == 5454 && f.sats == 8);

    /* DCS: la palabra debe ser divisible por el polinomio Golay */
    uint32_t w = dcs_golay_word(0023);
    printf("DCS 023 golay = 0x%06X\n", (unsigned)w);
    uint16_t lo, hi; assert(dtmf_freqs('5', &lo, &hi) && lo == 770 && hi == 1336);
    puts("OK");
    return 0;
}
