/* Audio local: PWM de TMR13 canal 1 en PA6 (AF9) -> amplificador (enable PB2).
 * Pitidos con PWM 50 %; mensajes de voz con PWM a ~94 kHz y TMR2 a 8 kHz
 * cargando la muestra en C1DT, igual que el firmware original.               */
#include "audio.h"
#include "board.h"
#include "system.h"
#include "spiflash.h"
#include "layout.h"

static volatile uint8_t amp_req;
static volatile uint16_t beep_left;
static uint8_t amp_delay;

/* Reproduccion de voz: doble buffer leido de la flash SPI desde el bucle principal */
#define VBUF 512
static uint8_t vbuf[2][VBUF];
static volatile uint8_t vplaying, vcur, vfill_needed;
static volatile uint16_t vpos;
static uint32_t vaddr, vleft;

void audio_amp_request(uint8_t src, int on)
{
    if (on) { amp_req |= src; amp_delay = 30; }
    else {
        amp_req &= (uint8_t)~src;
        if (!amp_req) gpio_clr(PIN_SPK_AMP);
    }
}

static void pwm_mode_tone(uint16_t hz)
{
    TMR13->ctrl1 = 0;
    TMR13->div = (uint16_t)(TMR_HZ / 256u / hz - 1u);
    TMR13->pr = 255;
    TMR13->c1dt = 128;
    TMR13->cm1 = (6u << 4) | (1u << 3);      /* PWM modo A, precarga */
    TMR13->cctrl = 1u;                        /* c1en */
    TMR13->swevt = 1;
    TMR13->ctrl1 = 1u | (1u << 7);
}

static void pwm_off(void)
{
    TMR13->ctrl1 = 0;
    TMR13->cctrl = 0;
}

void audio_init(void)
{
    CRM_APB1EN |= CRM_APB1EN_TMR13 | CRM_APB1EN_TMR2;
    gpio_clr(PIN_SPK_AMP);
    gpio_mode(PIN_SPK_AMP, GPIO_OUT, GPIO_NOPULL);
    gpio_af(PIN_BEEP_PWM, 9);
    /* TMR2: 8 kHz para muestras de voz (48 MHz / 12 / 500) */
    TMR2->div = 11;
    TMR2->pr = 499;
    TMR2->iden = 1;
    nvic_enable(IRQ_TMR2, 0);
}

void audio_tone(uint16_t hz)
{
    if (hz) { pwm_mode_tone(hz); audio_amp_request(AMP_TONE, 1); }
    else { pwm_off(); audio_amp_request(AMP_TONE, 0); }
}

void audio_beep(uint16_t hz, uint16_t ms)
{
    if (vplaying) return;
    pwm_mode_tone(hz);
    beep_left = ms;
    audio_amp_request(AMP_BEEP, 1);
}

void audio_tick_1ms(void)
{
    if (amp_delay && --amp_delay == 0 && amp_req) gpio_set(PIN_SPK_AMP);
    if (beep_left && --beep_left == 0) { pwm_off(); audio_amp_request(AMP_BEEP, 0); }
    if (vplaying && vfill_needed) { /* el relleno se hace en audio_voice_service */ }
}

void TMR2_GLOBAL_IRQHandler(void)
{
    TMR2->ists = 0;
    if (!vplaying) return;
    TMR13->c1dt = vbuf[vcur][vpos];
    if (++vpos >= VBUF) {
        vpos = 0;
        vcur ^= 1;
        if (vfill_needed & (1u << vcur)) { vplaying = 0; } /* no llego el buffer: cortar */
        vfill_needed |= (uint8_t)(1u << (vcur ^ 1));
    }
}

static void fill(int idx)
{
    uint32_t n = vleft < VBUF ? vleft : VBUF;
    if (n) spiflash_read(vaddr, vbuf[idx], n);
    for (uint32_t i = n; i < VBUF; i++) vbuf[idx][i] = 0x80;
    vaddr += n; vleft -= n;
    vfill_needed &= (uint8_t)~(1u << idx);
    if (!n) vplaying = 0;
}

/* Indice instalado por tools/install_resources.py: "VIDX" + u16 n + n x {u32 off, u32 len} */
void audio_voice_play(uint8_t clip)
{
    uint8_t h[6], e[8];
    spiflash_read(FLASH_VOICE_INDEX, h, 6);
    if (h[0] != 'V' || h[1] != 'I' || h[2] != 'D' || h[3] != 'X') return;
    if (clip >= (h[4] | (h[5] << 8))) return;
    spiflash_read(FLASH_VOICE_INDEX + 6 + clip * 8u, e, 8);
    vaddr = FLASH_VOICE + (e[0] | e[1] << 8 | (uint32_t)e[2] << 16 | (uint32_t)e[3] << 24);
    vleft = e[4] | e[5] << 8 | (uint32_t)e[6] << 16 | (uint32_t)e[7] << 24;
    if (vleft == 0 || vleft > 0x80000) return;
    vfill_needed = 0; vcur = 0; vpos = 0;
    fill(0); fill(1);
    TMR13->ctrl1 = 0;
    TMR13->div = 1;               /* ~94 kHz de portadora PWM */
    TMR13->pr = 255;
    TMR13->c1dt = 128;
    TMR13->cm1 = (6u << 4) | (1u << 3);
    TMR13->cctrl = 1u;
    TMR13->ctrl1 = 1u | (1u << 7);
    audio_amp_request(AMP_VOICE, 1);
    vplaying = 1;
    TMR2->ctrl1 = 1;
}

/* Debe llamarse desde el bucle principal: rellena buffers de voz */
int audio_voice_busy(void)
{
    if (vplaying) {
        for (int i = 0; i < 2; i++)
            if (vfill_needed & (1u << i)) fill(i);
    } else if (TMR2->ctrl1 & 1u) {
        TMR2->ctrl1 = 0;
        pwm_off();
        audio_amp_request(AMP_VOICE, 0);
    }
    return vplaying;
}

void audio_voice_stop(void) { vplaying = 0; audio_voice_busy(); }
