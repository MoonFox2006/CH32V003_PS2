#pragma once

#include <stdbool.h>
#include <ch32v00x.h>

#define PS2_RCC     RCC_APB2Periph_GPIOD
#define PS2_GPIO    GPIOD
#define PS2_EXTI    GPIO_PortSourceGPIOD

#define DAT_PIN     2
#define CLK_PIN     3

void kbd_init(void);
bool kbd_avail(void);
char kbd_get(void);
