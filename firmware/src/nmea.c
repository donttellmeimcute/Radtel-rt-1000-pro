/* Parser NMEA 0183 minimo: RMC y GGA (cualquier talker: GP, GN, BD...). */
#include "nmea.h"
#include <string.h>

static char buf[96];
static uint8_t len;

static int hexval(char c) { return c >= 'A' ? (c - 'A' + 10) : (c - '0'); }

static const char *field(const char *s, int n)
{
    while (n-- > 0) { s = strchr(s, ','); if (!s) return ""; s++; }
    return s;
}

static int32_t parse_fixed(const char *s, int decimals)
{
    int32_t v = 0; int neg = 0, d = -1;
    if (*s == '-') { neg = 1; s++; }
    for (; *s && *s != ',' && *s != '*'; s++) {
        if (*s == '.') { d = 0; continue; }
        if (*s < '0' || *s > '9') break;
        if (d >= decimals) continue;
        v = v * 10 + (*s - '0');
        if (d >= 0) d++;
    }
    if (d < 0) d = 0;
    while (d++ < decimals) v *= 10;
    return neg ? -v : v;
}

/* ddmm.mmmm -> grados*1e7 */
static int32_t parse_coord(const char *s, char hemi)
{
    int32_t raw = parse_fixed(s, 5);           /* ddmm*1e5 + mmmmm */
    int32_t deg = raw / 10000000;
    int32_t min_e5 = raw % 10000000;            /* minutos * 1e5 */
    int32_t v = deg * 10000000 + (int32_t)((int64_t)min_e5 * 100 / 60);
    return (hemi == 'S' || hemi == 'W') ? -v : v;
}

static int two(const char *s) { return (s[0] - '0') * 10 + (s[1] - '0'); }

static void parse(gps_fix_t *f)
{
    const char *s = buf + 3;   /* saltar "$GP" */
    if (!strncmp(s, "RMC,", 4)) {
        const char *t = field(buf, 1), *st = field(buf, 2), *d = field(buf, 9);
        if (t[0] && t[1]) { f->hh = two(t); f->mm = two(t + 2); f->ss = two(t + 4); }
        f->valid = (*st == 'A');
        if (f->valid) {
            f->lat_e7 = parse_coord(field(buf, 3), *field(buf, 4));
            f->lon_e7 = parse_coord(field(buf, 5), *field(buf, 6));
            f->speed_kmh10 = (uint16_t)(parse_fixed(field(buf, 7), 1) * 1852 / 1000); /* nudos->km/h */
            f->course_deg10 = (uint16_t)parse_fixed(field(buf, 8), 1);
        }
        if (d[0] && d[5]) { f->day = two(d); f->mon = two(d + 2); f->year = two(d + 4); }
    } else if (!strncmp(s, "GGA,", 4)) {
        f->sats = (uint8_t)parse_fixed(field(buf, 7), 0);
        f->alt_dm = parse_fixed(field(buf, 9), 1);
    }
}

int nmea_feed(gps_fix_t *fix, char c)
{
    if (c == '$') { len = 0; }
    if (len >= sizeof(buf) - 1) { len = 0; return 0; }
    if (c == '\r' || c == '\n') {
        if (len < 8 || buf[0] != '$') { len = 0; return 0; }
        buf[len] = 0; len = 0;
        char *star = strchr(buf, '*');
        if (!star || !star[1] || !star[2]) return 0;
        uint8_t ck = 0;
        for (char *p = buf + 1; p < star; p++) ck ^= (uint8_t)*p;
        if (ck != (hexval(star[1]) << 4 | hexval(star[2]))) return 0;
        parse(fix);
        return 1;
    }
    buf[len++] = c;
    return 0;
}
