/* The driver of the Bluetooth bring-up: run our own code on the Bluetooth CPU.
 * Each call below is one routine of the tutorial; each panics on failure. */
#include "rpi.h"
#include "pl011.h"
#include "bt.h"

/* chip.S, assembled for the radio's Cortex-M3 (see the Makefile) */
static const uint8_t chip[] = {
#include "chip_image.h"
};

void notmain(void) {
    pl011_up(115200, CR_RTS);              /* 1. the PL011: 115200 8N1, RTS on   */
    bt_reset(10);                          /* 2. reset the Bluetooth half        */
    unsigned ready = bt_connect(500 * 1000);  /* 3. connect; HCI Reset answered  */
    trace("the radio's ROM answers HCI Reset [RTS after %d us]\n", ready);

    unsigned n = timer_get_usec() * 2654435761u;
    unsigned answer = bt_run(chip, sizeof chip, n);    /* 4. run chip.S on the M3 */
    if (answer != 3 * n + 7)
        panic("the Bluetooth CPU answered %x for n=%x", answer, n);
    trace("the Bluetooth CPU computed 3n+7 [n=%x answer=%x]\n", n, answer);
}
