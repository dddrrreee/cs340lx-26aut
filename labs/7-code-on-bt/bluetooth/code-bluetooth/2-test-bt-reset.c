/* Test bt_reset(): does the Bluetooth half really restart, and
 * how long does it take? Each pass marks the radio's RAM, resets the radio
 * (or not, for the control), and checks two independent witnesses:
 *  - time: our RTS stays asserted throughout, so the radio asserts its RTS
 *    as soon as its ROM is running again: about 34 ms after a restart, but
 *    within microseconds if the radio never stopped;
 *  - memory: LOAD is RAM the ROM zeroes when it starts, so a word we wrote
 *    there survives only if the ROM did not restart.
 * A hold of 0 skips the reset: the control, where both witnesses must say
 * "kept running". */
#include "rpi.h"
#include "pl011.h"
#include "bt.h"

/* hold times to try, in ms; 0 = no reset. All but 50 appear twice, in
 * mixed order, so a result that depends on the previous pass would show. */
static const unsigned holds[] = { 0, 50, 10, 1, 0, 10, 1 };

/* microseconds until the radio asserts its RTS (FR_CTS); panics after
 * timeout_us */
static unsigned wait_rts(unsigned timeout_us) {
    unsigned start = timer_get_usec();
    while (!(GET32(UART_FR) & FR_CTS))
        if (timer_get_usec() - start > timeout_us)
            panic("the radio never asserted its RTS");
    return timer_get_usec() - start;
}

void notmain(void) {
    pl011_up(115200, CR_RTS);                  /* our RTS: asserted from here on */
    bt_reset(50);
    trace("first connect [RTS after %d us]\n", bt_connect(500 * 1000));

    for (unsigned i = 0; i < sizeof holds / sizeof holds[0]; i++) {
        unsigned hold = holds[i];

        /* 1. Mark the radio's RAM with a word only this pass writes (the
         *    pass number is in the low half). bt_write_ram reads it back,
         *    so the mark is known to be there. */
        unsigned mark = 0x5a5a0000 + i;
        bt_write_ram(LOAD, &mark, 4);

        /* 2. Reset the Bluetooth half, unless this is the control. */
        if (hold)
            bt_reset(hold);

        /* 3. Witness 1, time: how long until the radio says it is ready? */
        unsigned ready = wait_rts(500 * 1000);

        /* 4. Witness 2, memory: is the mark still there? Read it before any
         *    HCI command, because HCI Reset clears it too. */
        unsigned after;
        bt_read_ram(LOAD, &after, 4);

        /* 5. Put the ROM in a known state for the next pass, and record
         *    that HCI Reset also clears the word. */
        unsigned later;
        hci(HCI_RESET, 0, 0, 0, 0);
        bt_read_ram(LOAD, &later, 4);

        /* 6. Both witnesses must agree with what we did. A restart takes
         *    about 34,000 us and a running radio answers in 1-2 us, so the
         *    cut-offs of 10,000 and 1,000 us sit far from both. */
        int restarted = after == 0 && ready > 10000;
        int kept_running = after == mark && ready < 1000;
        if (hold ? !restarted : !kept_running)
            panic("hold %d ms: RTS after %d us, word %x", hold, ready, after);
        trace("hold %d ms: %s [RTS after %d us; word %x, then %x; %x after Reset]\n",
              hold, hold ? "restarted" : "kept running", ready, mark, after, later);
    }
}
