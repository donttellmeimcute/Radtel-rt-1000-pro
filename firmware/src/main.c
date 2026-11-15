/* Firmware libre para Radtel RT-1000 Pro (AT32F435 + BK4819 + Si4732). */
#include "system.h"
#include "board.h"
#include "lcd.h"
#include "keys.h"
#include "spiflash.h"
#include "settings.h"
#include "adc.h"
#include "audio.h"
#include "uart.h"
#include "i2c.h"
#include "radio.h"
#include "cps.h"
#include "app.h"
#include "layout.h"
#include "version.h"
#include <string.h>

uint16_t g_flash_id;
static const uint8_t bl_levels[5] = {0x3C, 0x41, 0x46, 0x4C, 0xFF};

void app_bl_apply(void) { lcd_backlight(bl_levels[g_set.brightness % 5]); }

static void boot_screen(void)
{
    int shown = 0;
    if (g_set.boot_logo) {
        /* Logo de 240x320 en la flash SPI (lo instala el CPS oficial) */
        static uint8_t buf[2048];
        spiflash_read(FLASH_BOOT_LOGO, buf, 16);
        int blank = 1;
        for (int i = 0; i < 16; i++) if (buf[i] != 0xFF) blank = 0;
        if (!blank) {
            lcd_stream_begin(0, 0, LCD_W, LCD_H);
            for (uint32_t o = 0; o < 240u * 320u * 2u; o += sizeof(buf)) {
                spiflash_read(FLASH_BOOT_LOGO + o, buf, sizeof(buf));
                lcd_stream_bytes(buf, sizeof(buf));
            }
            shown = 1;
        }
    }
    if (!shown) {
        lcd_clear(C_BLACK);
        lcd_text_center(120, g_set.welcome, &FONT_MEDIUM, C_YELLOW, C_BLACK);
        lcd_text_center(160, "Firmware libre " FW_VERSION, &FONT_SMALL, C_WHITE, C_BLACK);
    }
    app_bl_apply();
    if (g_set.voice) audio_voice_play(0);
    delay_ms(1200);
}

int main(void)
{
    CRM_AHBEN1 |= CRM_AHBEN1_GPIOA | CRM_AHBEN1_GPIOB | CRM_AHBEN1_GPIOC | CRM_AHBEN1_GPIOD | CRM_AHBEN1_GPIOF;
    gpio_set(PIN_POWER_HOLD);                       /* mantener la alimentacion */
    gpio_mode(PIN_POWER_HOLD, GPIO_OUT, GPIO_NOPULL);
    system_clock_init();
    delay_ms(50);

    keys_init();
    lcd_init();
    spiflash_init();
    g_flash_id = spiflash_id();
    settings_load();
    adc_init();
    audio_init();
    uart_pc_init(115200);
    i2c2_init();
    radio_init();
    cps_init();
    boot_screen();

    /* Pines SWD reutilizados como LEDs: igual que el original. Si se mantiene
     * presionado EXIT al encender se conservan para depuracion. */
    delay_ms(30);
    key_event_t e; int keep_swd = 0;
    while (keys_get(&e)) if (e.key == KEY_EXIT) keep_swd = 1;
    if (!keep_swd) {
        gpio_clr(PIN_LED_GREEN); gpio_mode(PIN_LED_GREEN, GPIO_OUT, GPIO_NOPULL);
        gpio_clr(PIN_LED_RED); gpio_mode(PIN_LED_RED, GPIO_OUT, GPIO_NOPULL);
    }

    app_init();
    for (;;) app_loop();
}
