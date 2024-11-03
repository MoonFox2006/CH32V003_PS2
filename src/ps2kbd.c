#include <ch32v00x_rcc.h>
#include <ch32v00x_gpio.h>
#include <ch32v00x_exti.h>
#include "ps2kbd.h"

#define EXTCODE     0xE0
#define RELCODE     0xF0

#define LSHIFTCODE  0x12
#define RSHIFTCODE  0x59
#define CAPSCODE    0x58

#define F7CODE      0x83
#define F7RECODE    0xFF

#define KEYBUF_SIZE 16

static const uint8_t SCANCODES[] = {
    0x0D, '\t', '\t', '\t', '\t', // TAB
    0x0E, '`', '~', '\xB8', '\xA8', // 'ё'
	0x15, 'q', 'Q', '\xE9', '\xC9', // 'й'
	0x16, '1', '!', '1', '!',
	0x1A, 'z', 'Z', '\xFF', '\xDF', // 'я'
	0x1B, 's', 'S', '\xFB', '\xDB', // 'ы'
	0x1C, 'a', 'A', '\xF4', '\xD4', // 'ф'
	0x1D, 'w', 'W', '\xF6', '\xD6', // 'ц'
	0x1E, '2', '@', '2', '"', // '"'
	0x21, 'c', 'C', '\xF1', '\xD1', // 'с'
	0x22, 'x', 'X', '\xF7', '\xD7', // 'ч'
	0x23, 'd', 'D', '\xE2', '\xC2', // 'в'
	0x24, 'e', 'E', '\xF3', '\xD3', // 'у'
	0x25, '4', '$', '4', ';', // ';'
	0x26, '3', '#', '3', '\xB9', // '№'
	0x29, ' ', ' ', ' ', ' ', // SPACE
	0x2A, 'v', 'V', '\xEC', '\xCC', // 'м'
	0x2B, 'f', 'F', '\xE0', '\xC0', // 'а'
	0x2C, 't', 'T', '\xE5', '\xC5', // 'е'
	0x2D, 'r', 'R', '\xEA', '\xCA', // 'к'
	0x2E, '5', '%', '5', '%',
	0x31, 'n', 'N', '\xF2', '\xD2', // 'т'
	0x32, 'b', 'B', '\xE8', '\xC8', // 'и'
	0x33, 'h', 'H', '\xF0', '\xD0', // 'р'
	0x34, 'g', 'G', '\xEF', '\xCF', // 'п'
	0x35, 'y', 'Y', '\xED', '\xCD', // 'н'
	0x36, '6', '^', '6', ':', // ':'
	0x3A, 'm', 'M', '\xFC', '\xDC', // 'ь'
	0x3B, 'j', 'J', '\xEE', '\xCE', // 'о'
	0x3C, 'u', 'U', '\xE3', '\xC3', // 'г'
	0x3D, '7', '&', '7', '?', // '?'
	0x3E, '8', '*', '8', '*',
	0x41, ',', '<', '\xE1', '\xC1', // 'б'
	0x42, 'k', 'K', '\xEB', '\xCB', // 'л'
	0x43, 'i', 'I', '\xF8', '\xD8', // 'ш'
	0x44, 'o', 'O', '\xF9', '\xD9', // 'щ'
	0x45, '0', ')', '0', ')',
	0x46, '9', '(', '9', '(',
	0x49, '.', '>', '\xFE', '\xDE', // 'ю'
	0x4A, '/', '?', '.', ',', // '.', ','
	0x4B, 'l', 'L', '\xE4', '\xC4', // 'д'
	0x4C, ';', ':', '\xE6', '\xC6', // 'ж'
	0x4D, 'p', 'P', '\xE7', '\xC7', // 'з'
	0x4E, '-', '_', '-', '_',
	0x52, '\'', '"', '\xFD', '\xDD', // 'э'
	0x54, '[', '{', '\xF5', '\xD5', // 'х'
	0x55, '=', '+', '=', '+',
	0x5A, '\r', '\r', '\r', '\r',
	0x5B, ']', '}', '\xFA', '\xDA', // 'ъ'
	0x5D, '\\', '|', '\\', '/', // '\\', '/'
	0x66, '\b', '\b', '\b', '\b', // BS
};

static volatile char _buf[KEYBUF_SIZE];
static volatile uint8_t _buflen = 0;
static volatile uint8_t _bufpos = 0;

void kbd_init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = { 0 };
    EXTI_InitTypeDef EXTI_InitStructure = { 0 };
    NVIC_InitTypeDef NVIC_InitStructure = { 0 };

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | PS2_RCC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = (1 << DAT_PIN) | (1 << CLK_PIN);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // GPIO_Mode_IPU
    GPIO_Init(PS2_GPIO, &GPIO_InitStructure);

    GPIO_EXTILineConfig(PS2_EXTI, CLK_PIN);
    EXTI_InitStructure.EXTI_Line = 1 << CLK_PIN;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = EXTI7_0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

inline bool __attribute__((always_inline)) kbd_avail(void) {
    return _buflen != 0;
}

char kbd_get(void) {
    if (_buflen) {
        char result;

        NVIC_DisableIRQ(EXTI7_0_IRQn);
        result = _buf[(_bufpos - _buflen + KEYBUF_SIZE) & (KEYBUF_SIZE - 1)];
        --_buflen;
        NVIC_EnableIRQ(EXTI7_0_IRQn);
        return result;
    }
    return 0;
}

/*********************************************************************
 * @fn      EXTI0_IRQHandler
 *
 * @brief   This function handles EXTI0 Handler.
 *
 * @return  none
 */
void __attribute__((interrupt("WCH-Interrupt-fast"))) EXTI7_0_IRQHandler(void) {
    static uint16_t scan = 0;
    static uint8_t bits = 0;
    static struct {
        bool ext : 1;
        bool rel : 1;
        bool lshift : 1;
        bool rshift : 1;
        bool caps : 1;
        bool rus : 1;
    } flags = { false, false, false, false, false, false };

    if (EXTI->INTFR & (1 << CLK_PIN)) {
        if (bits) {
            scan >>= 1;
            if ((PS2_GPIO->INDR >> DAT_PIN) & 0x01)
                scan |= 0x8000;
            if (++bits >= 11) {
                if (scan & 0x8000) { // Stop bit (1)
                    scan >>= 6;
                    if ((uint8_t)scan == EXTCODE)
                        flags.ext = true;
                    else if ((uint8_t)scan == RELCODE)
                        flags.rel = true;
                    else {
                        if (flags.rel) { // Key released
                            if ((uint8_t)scan == LSHIFTCODE)
                                flags.lshift = false;
                            else if ((uint8_t)scan == RSHIFTCODE)
                                flags.rshift = false;
                        } else { // Key pressed
                            if ((uint8_t)scan == LSHIFTCODE) {
                                flags.lshift = true;
                                if (flags.rshift)
                                    flags.rus = ! flags.rus;
                            } else if ((uint8_t)scan == RSHIFTCODE) {
                                flags.rshift = true;
                                if (flags.lshift)
                                    flags.rus = ! flags.rus;
                            } else if ((uint8_t)scan == CAPSCODE)
                                flags.caps = ! flags.caps;
                            else {
                                if (flags.ext)
                                    scan |= 0x80;
                                else if ((uint8_t)scan == F7CODE)
                                    scan = F7RECODE;
                                for (uint16_t i = 0; i < sizeof(SCANCODES); i += 5) {
                                    if (SCANCODES[i] == (uint8_t)scan) {
                                        if (_buflen < KEYBUF_SIZE) {
                                            _buf[_bufpos] = SCANCODES[i + 2 * flags.rus + 1 + ((flags.lshift || flags.rshift) ^ flags.caps)];
                                            ++_buflen;
                                            if (++_bufpos >= KEYBUF_SIZE)
                                                _bufpos = 0;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                        flags.ext = false;
                        flags.rel = false;
                    }
                }
                scan = 0;
                bits = 0;
            }
        } else { // _bits == 0
            if (! ((PS2_GPIO->INDR >> DAT_PIN) & 0x01)) // Start bit (0)
                ++bits;
        }
        EXTI->INTFR = 1 << CLK_PIN;
    }
}
