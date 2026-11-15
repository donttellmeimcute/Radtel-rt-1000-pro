#pragma once
#include "at32f435.h"

enum { GPIO_IN = 0, GPIO_OUT = 1, GPIO_AF = 2, GPIO_ANALOG = 3 };
enum { GPIO_NOPULL = 0, GPIO_PULLUP = 1, GPIO_PULLDOWN = 2 };

typedef struct { gpio_t *port; uint8_t pin; } pin_t;

static inline void gpio_mode(pin_t p, int mode, int pull)
{
    uint32_t s = p.pin * 2u;
    p.port->cfgr = (p.port->cfgr & ~(3u << s)) | ((uint32_t)mode << s);
    p.port->pull = (p.port->pull & ~(3u << s)) | ((uint32_t)pull << s);
    p.port->omode &= ~(1u << p.pin);
}
static inline void gpio_af(pin_t p, int af)
{
    volatile uint32_t *r = p.pin < 8 ? &p.port->muxl : &p.port->muxh;
    uint32_t s = (p.pin & 7u) * 4u;
    *r = (*r & ~(0xFu << s)) | ((uint32_t)af << s);
    gpio_mode(p, GPIO_AF, GPIO_NOPULL);
}
static inline void gpio_set(pin_t p)   { p.port->scr = 1u << p.pin; }
static inline void gpio_clr(pin_t p)   { p.port->clr = 1u << p.pin; }
static inline void gpio_write(pin_t p, int v) { if (v) gpio_set(p); else gpio_clr(p); }
static inline int  gpio_read(pin_t p)  { return (p.port->idt >> p.pin) & 1u; }
