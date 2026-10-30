/* hello.c: our first C program on the radio's Bluetooth CPU. It never runs on
 * the Pi.
 *
 * It asks the radio's ROM to send one HCI event to the Pi, carrying our text. The
 * ROM's event routine, ROM_SEND_EVENT below (0x361e, called with bit 0 set), takes
 * a pointer to an event: {code, length, payload}. It copies the event before it
 * returns, and queues it for the radio's UART; the ROM uses the same routine for its
 * own Command Complete events (lab card 04). We use code 0xff, the event code the
 * Bluetooth spec leaves to vendors. The address is right only for this ROM, so
 * 4-bt-hello.c checks the ROM's version before it loads this program. */
#include <stdint.h>

enum { ROM_SEND_EVENT = 0x361e | 1 };     /* bit 0 set: call it as Thumb code */

static void send_event(const uint8_t *event) {
    ((void (*)(const uint8_t *))ROM_SEND_EVENT)(event);
}

void radio_main(void) {
    static const char text[] = "hello from the Bluetooth CPU";
    uint8_t event[2 + sizeof text];

    event[0] = 0xff;                       /* a vendor event               */
    event[1] = sizeof text;                /* payload bytes, NUL included  */
    for (unsigned i = 0; i < sizeof text; i++)
        event[2 + i] = text[i];
    send_event(event);
}
