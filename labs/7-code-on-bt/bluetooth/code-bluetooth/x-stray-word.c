/* Experiment (not part of the tutorial): the stray word that can follow
 * pl011_up()'s enable. Which write brings it, and does it come every time?
 * Each variant runs 4 times; each window logs every word received in the
 * 2 ms after the variant's last write, with its arrival time. */
#include "rpi.h"
#include "pl011.h"

enum {
    UART_RSR = 0x20201004, UART_RIS = 0x2020103c,
    GPFSEL1 = 0x20200004, GPFSEL3 = 0x2020000c,
};

static void window(const char *what, unsigned try) {
    unsigned w[8], t[8], k = 0, start = timer_get_usec(), now;
    while ((now = timer_get_usec()) - start < 2000)
        if (!(GET32(UART_FR) & FR_RXFE)) {
            unsigned x = GET32(UART_DR);
            if (k < 8) {
                w[k] = x;
                t[k++] = now - start;
            }
        }
    if (!k)
        trace("%s, try %d: none\n", what, try);
    for (unsigned i = 0; i < k; i++)
        trace("%s, try %d: word %x at %d us\n", what, try, w[i], t[i]);
}

/* pl011_up's writes up to the enable, without its wait and drain */
static void setup(unsigned cr) {
    PUT32(UART_CR, 0);
    PUT32(UART_IBRD, 26);
    PUT32(UART_FBRD, 3);
    PUT32(UART_LCRH, LCRH_WLEN_8 | LCRH_FEN);
    PUT32(UART_CR, cr);
}

void notmain(void) {
    trace("entry: CR=%x FR=%x RIS=%x RSR=%x LCRH=%x GPFSEL1=%x GPFSEL3=%x\n",
          GET32(UART_CR), GET32(UART_FR), GET32(UART_RIS), GET32(UART_RSR),
          GET32(UART_LCRH), GET32(GPFSEL1), GET32(GPFSEL3));
    unsigned on = CR_UARTEN | CR_TXE | CR_RXE;

    for (unsigned i = 0; i < 4; i++) {
        setup(on | CR_LBE);
        window("A: enable with loopback", i);
    }
    for (unsigned i = 0; i < 4; i++) {
        setup(on);
        window("B: enable, no loopback, RX unrouted", i);
    }
    for (unsigned i = 0; i < 4; i++) {
        setup(CR_LBE);
        window("C1: loopback set, UART still off", i);
        PUT32(UART_CR, on | CR_LBE);
        window("C2: then enable", i);
    }
    /* RX routed to GPIO15, which the host's adapter holds high */
    gpio_set_function(15, GPIO_FUNC_ALT0);
    for (unsigned i = 0; i < 4; i++) {
        setup(on);
        window("D: enable, no loopback, RX on GPIO15", i);
    }
    for (unsigned i = 0; i < 4; i++) {
        setup(on | CR_LBE);
        window("E: enable with loopback, RX on GPIO15", i);
    }
    gpio_set_function(15, GPIO_FUNC_ALT5);
    trace("end: RIS=%x\n", GET32(UART_RIS));
}
