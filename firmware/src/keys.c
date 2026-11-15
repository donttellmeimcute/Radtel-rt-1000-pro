/* Teclado 4x4, PTT, encoder rotatorio y teclas laterales por ADC.
 * La asignacion fisica de 0x0A-0x0F (MENU/UP/DOWN/EXIT/'*'/'#') es la del
 * firmware original; el rotulo exacto de cada tecla debe confirmarse en el radio. */
#include "keys.h"
#include "board.h"

static const pin_t rows[4] = {{GPIOA, 9}, {GPIOF, 6}, {GPIOB, 6}, {GPIOB, 7}};
static const pin_t cols[4] = {{GPIOC, 10}, {GPIOC, 11}, {GPIOC, 12}, {GPIOD, 2}};
static const uint8_t keymap[4][4] = {
    {KEY_1, KEY_4, KEY_7, KEY_STAR},
    {KEY_2, KEY_5, KEY_8, KEY_0},
    {KEY_3, KEY_6, KEY_9, KEY_HASH},
    {KEY_MENU, KEY_UP, KEY_DOWN, KEY_EXIT},
};

#define QSZ 16
static key_event_t q[QSZ];
static volatile uint8_t qh, qt;
static uint8_t row, matrix_key = KEY_NONE, scan_key = KEY_NONE;
static uint8_t cur = KEY_NONE;
static uint16_t held_ms, debounce;
static volatile int enc_acc;
static uint8_t enc_last;
static volatile uint8_t side_key = KEY_NONE;
static uint8_t ptt_state;

static void push(uint8_t k, key_ev_t ev)
{
    uint8_t n = (uint8_t)((qh + 1) % QSZ);
    if (n != qt) { q[qh].key = k; q[qh].ev = ev; qh = n; }
}

void keys_init(void)
{
    CRM_AHBEN1 |= CRM_AHBEN1_GPIOA | CRM_AHBEN1_GPIOB | CRM_AHBEN1_GPIOC | CRM_AHBEN1_GPIOD | CRM_AHBEN1_GPIOF;
    for (int i = 0; i < 4; i++) {
        gpio_set(rows[i]);
        gpio_mode(rows[i], GPIO_OUT, GPIO_NOPULL);
        gpio_mode(cols[i], GPIO_IN, GPIO_PULLUP);
    }
    gpio_mode(PIN_PTT, GPIO_IN, GPIO_PULLUP);
    gpio_mode(PIN_ENC_A, GPIO_IN, GPIO_PULLUP);
    gpio_mode(PIN_ENC_B, GPIO_IN, GPIO_PULLUP);
    enc_last = (uint8_t)(gpio_read(PIN_ENC_A) << 1 | gpio_read(PIN_ENC_B));
    gpio_clr(rows[0]);
}

/* Tabla de transiciones de cuadratura: +1 / -1 / 0 */
static const int8_t qdec[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

void keys_tick_1ms(void)
{
    /* Encoder */
    uint8_t e = (uint8_t)(gpio_read(PIN_ENC_A) << 1 | gpio_read(PIN_ENC_B));
    static int8_t sub;
    if (e != enc_last) {
        sub += qdec[(enc_last << 2) | e];
        enc_last = e;
        if (e == 3) {                    /* posicion de reposo: 1 paso = 4 transiciones */
            if (sub >= 2) enc_acc++;
            else if (sub <= -2) enc_acc--;
            sub = 0;
        }
    }

    /* Matriz: una fila por milisegundo */
    for (int c = 0; c < 4; c++)
        if (!gpio_read(cols[c])) scan_key = keymap[row][c];
    gpio_set(rows[row]);
    row = (uint8_t)((row + 1) & 3);
    if (row == 0) { matrix_key = scan_key; scan_key = KEY_NONE; }
    gpio_clr(rows[row]);

    uint8_t k = matrix_key != KEY_NONE ? matrix_key : side_key;

    /* PTT con antirrebote propio */
    uint8_t p = !gpio_read(PIN_PTT);
    static uint8_t pcnt;
    if (p != ptt_state) {
        if (++pcnt > 20) { ptt_state = p; pcnt = 0; push(KEY_PTT, p ? EV_PRESS : EV_RELEASE); }
    } else pcnt = 0;

    if (k != cur) {
        if (++debounce < 20) return;
        if (cur != KEY_NONE) push(cur, EV_RELEASE);
        cur = k; held_ms = 0; debounce = 0;
        if (k != KEY_NONE) push(k, EV_PRESS);
        return;
    }
    debounce = 0;
    if (cur == KEY_NONE) return;
    held_ms++;
    if (held_ms == 800) push(cur, EV_LONG);
    else if (held_ms > 800 && (held_ms - 800) % 150 == 0) push(cur, EV_REPEAT);
}

int keys_get(key_event_t *ev)
{
    if (qh == qt) return 0;
    *ev = q[qt];
    qt = (uint8_t)((qt + 1) % QSZ);
    return 1;
}

int keys_ptt_pressed(void) { return ptt_state; }

int encoder_take(void)
{
    int v = enc_acc;
    enc_acc -= v;
    return v;
}

/* Umbrales del original: <0x75D SIDE2(0x17), <0xA9D SIDE1(0x16), <0xDD6 ALARM(0x18) */
void keys_set_sidekey_adc(uint16_t raw)
{
    side_key = raw < 0x75D ? KEY_SIDE2 : raw < 0xA9D ? KEY_SIDE1 : raw < 0xDD6 ? KEY_ALARM : KEY_NONE;
}
