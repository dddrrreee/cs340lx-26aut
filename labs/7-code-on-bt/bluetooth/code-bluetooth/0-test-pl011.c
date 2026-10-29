/* Test pl011_up() alone: no radio and no wire. With CR_LBE
 * (loopback enable) the PL011's transmitter feeds its own receiver, so each
 * byte written to DR must come back. For each rate: send 64 bytes, check
 * each one, and time the 63 frames between the first arrival and the last.
 * The time must match what the divisor predicts. At entry the PL011
 * already runs at 115200, so only the other rates show the writes take
 * effect; 115385 differs from 115200 only in FBRD (3 -> 0). */
#include "rpi.h"
#include "pl011.h"

static const unsigned rates[] = { 115200, 115385, 57600, 230400, 921600 };

void notmain(void) {
    trace("at entry: CR=%x IBRD=%d FBRD=%d LCRH=%x\n",
          GET32(UART_CR), GET32(UART_IBRD), GET32(UART_FBRD), GET32(UART_LCRH));

    for (unsigned i = 0; i < sizeof rates / sizeof rates[0]; i++) {
        unsigned baud = rates[i], div = pl011_div64(baud);
        pl011_up(baud, CR_LBE);

        unsigned sent = 0, got = 0, first = 0, last = 0;
        unsigned start = timer_get_usec();
        while (got < 64) {
            if (timer_get_usec() - start > 100000)
                panic("%d baud: only %d of 64 bytes came back", baud, got);
            while (sent < 64 && !(GET32(UART_FR) & FR_TXFF))  /* keep TX full */
                PUT32(UART_DR, sent++);
            if (!(GET32(UART_FR) & FR_RXFE)) {
                last = timer_get_usec();
                if (got == 0)
                    first = last;
                unsigned w = GET32(UART_DR);     /* data, and no error flags */
                if (w != got)
                    panic("%d baud: byte %d came back as %x", baud, got, w);
                got++;
            }
        }
        /* 63 frames x 10 bits x 16 UARTCLK cycles x div/64; UARTCLK is 48 per us */
        unsigned want = (630 * div + 96) / 192, took = last - first;
        if (took + 3 < want || took > want + 3)
            panic("%d baud: 63 frames took %d us, not %d", baud, took, want);
        trace("%d baud (IBRD %d, FBRD %d): 64 bytes back, 63 frames in %d us "
              "[measured %d us]\n", baud, div >> 6, div & 63, want, took);
    }
}
