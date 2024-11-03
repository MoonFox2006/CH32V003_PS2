#include "debug.h"
#include "ps2kbd.h"

int main(void) {
    SystemCoreClockUpdate();

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

    Delay_Init();
    USART_Printf_Init(115200);

    printf("PS/2 keyboard demo\n");
    kbd_init();

    while (1) {
        if (kbd_avail()) {
            char ch = kbd_get();

            putchar(ch);
            if (ch == '\r')
                putchar('\n');
        }
    }
}
