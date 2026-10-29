/* Hello world from the Bluetooth CPU. Bring the radio up as the driver does, check
 * that its ROM is the one whose event routine hello.c calls, load hello.c's bytes,
 * launch them, and print the text the radio sends back as an HCI event. */
#include "rpi.h"
#include "pl011.h"
#include "bt.h"

/* hello.c, start.S and hello.ld, compiled for the radio's Cortex-M3 (see the
 * Makefile) */
static const uint8_t hello[] = {
#include "hello_image.h"
};

/* The tested ROM's answer to HCI Read_Local_Version_Information (BT Part E 7.4 and
 * 7.4.1): HCI_Version 7, HCI_Subversion 0, LMP_Version 7, Company_Identifier 0x000f
 * (Broadcom, in Bluetooth's Assigned Numbers), LMP_Subversion 0x2209. hello.c's
 * ROM_SEND_EVENT (0x361e | 1) is right only for this ROM. */
enum { READ_LOCAL_VERSION = 0x1001 };
enum { RADIO_STACK_TOP = 0x21d800 };          /* hello.ld's __stack_top */
static const uint8_t ROM_VERSION[8] = {
    0x07, 0x00, 0x00, 0x07, 0x0f, 0x00, 0x09, 0x22
};

/* the next byte from the radio; panics if none comes within 1 s */
static uint8_t next_byte(void) {
    unsigned start = timer_get_usec();
    while (GET32(UART_FR) & FR_RXFE)
        if (timer_get_usec() - start > 1000000)
            panic("no byte from the radio for 1 s");
    return GET32(UART_DR);
}

/* Read one HCI event: e[0] its code, e[1] its length, then the payload. Returns the
 * code. */
static unsigned read_event(uint8_t e[258]) {
    while (next_byte() != 4)                   /* skip any byte before an event */
        ;
    e[0] = next_byte();
    e[1] = next_byte();
    for (unsigned i = 0; i < e[1]; i++)
        e[2 + i] = next_byte();
    return e[0];
}

void notmain(void) {
    pl011_up(115200, CR_RTS);                  /* sections 1-3, as in the driver */
    bt_reset(10);
    bt_connect(500 * 1000);

    uint8_t v[8];                              /* refuse any other ROM */
    hci(READ_LOCAL_VERSION, 0, 0, v, 8);
    for (unsigned i = 0; i < 8; i++)
        if (v[i] != ROM_VERSION[i])
            panic("not the tested ROM: version byte %d is %x", i, v[i]);

    bt_write_ram(LOAD, hello, sizeof hello);   /* load and run hello.c (section 4) */
    bt_launch_ram(LOAD | 1);

    /* bt_launch_ram already read Launch_RAM's Command Complete, which the ROM queues
     * before it calls our code. Our event follows: skip anything else until the
     * vendor event arrives. */
    uint8_t e[258];
    while (read_event(e) != 0xff)
        ;
    e[2 + e[1]] = 0;                           /* end the text, NUL or not */
    trace("the Bluetooth CPU says: %s [%d bytes]\n", (char *)e + 2, e[1]);

    hci(READ_LOCAL_VERSION, 0, 0, v, 8);       /* our program returned: the ROM */
    trace("the ROM still answers HCI\n");      /*   still serves commands       */

    /* radio_main built its event on its stack, so if it ran on its own stack, the
     * text is now in the RAM just below RADIO_STACK_TOP */
    uint8_t ram[64];
    bt_read_ram(RADIO_STACK_TOP - sizeof ram, ram, sizeof ram);
    unsigned found = 0;
    for (unsigned i = 0; i + 5 <= sizeof ram; i++)
        found |= ram[i] == 'h' && ram[i + 1] == 'e' && ram[i + 2] == 'l'
                 && ram[i + 3] == 'l' && ram[i + 4] == 'o';
    if (!found)
        panic("the text is not below %x: the stack switch did not happen",
              RADIO_STACK_TOP);
    trace("the text is in RAM just below %x: the program ran on its own stack\n",
          RADIO_STACK_TOP);
}
