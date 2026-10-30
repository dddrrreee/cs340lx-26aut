#include "rpi.h"
#include "pl011.h"
#include "bt.h"

/* bt_reset: restart the Bluetooth half, so its ROM boots from scratch.
 * BT_REG_ON low holds Bluetooth in reset, and high lets it boot. WL_REG_ON
 * stays high, and the chip's regulators stay on while either enable is
 * high, so this is not a power cycle: only Bluetooth restarts (CYW43438
 * p. 83). */
void bt_reset(unsigned hold_ms) {
    gpio_set_output(BT_REG_ON);
    gpio_set_off(BT_REG_ON);                   /* hold Bluetooth in reset   */
    delay_ms(hold_ms);
    gpio_set_on(BT_REG_ON);                    /* release it: the ROM boots */
}

/* bt_connect: connect the PL011 to the radio, wait up to `timeout_us` for
 * the radio to say it is ready, then send HCI Reset. Returns how long the
 * radio took. The radio says "ready" by asserting its RTS, which is our
 * CTS, and it does so only after it sees our RTS: call
 * pl011_up(115200, CR_RTS) first. */
unsigned bt_connect(unsigned timeout_us) {
    /* GPIO30-33 at ALT3: the PL011's CTS, RTS, TX and RX, wired to the radio */
    gpio_set_function(CTS0, GPIO_FUNC_ALT3);
    gpio_set_function(RTS0, GPIO_FUNC_ALT3);
    gpio_set_function(TXD0, GPIO_FUNC_ALT3);
    gpio_set_function(RXD0, GPIO_FUNC_ALT3);
    dev_barrier();

    unsigned start = timer_get_usec();
    while (!(GET32(UART_FR) & FR_CTS))         /* the radio's RTS, our CTS */
        if (timer_get_usec() - start > timeout_us)
            panic("the radio never asserted its RTS");
    unsigned ready_us = timer_get_usec() - start;
    hci(HCI_RESET, 0, 0, 0, 0);
    return ready_us;
}

/* the next received byte; panics if none comes within 1 s */
static uint8_t recv_byte(void) {
    unsigned start = timer_get_usec();
    while (GET32(UART_FR) & FR_RXFE)
        if (timer_get_usec() - start > 1000000)
            panic("no byte from the radio for 1 s");
    return GET32(UART_DR);
}

/* Send one HCI command and read its Command Complete event.
 *   command: 01, opcode (low byte first), parameter length, parameters
 *   event:   04, 0e, length, commands the radio will take next, opcode,
 *            status, then nret return bytes, which go to ret
 * (Bluetooth Core 5.4: 01 and 04, Vol 4 Part A Table 2.1; the packets,
 * Part E 5.2 (byte order), 5.4.1 and 5.4.4; Command Complete, 7.7.14.)
 * Panics unless the event completes this opcode with status 0. */
void hci(unsigned op, const void *p, unsigned n, void *ret, unsigned nret) {
    /* send the 4 header bytes, then the parameters, back to back, waiting
     * whenever the transmit FIFO is full */
    const uint8_t head[4] = {1, op, op >> 8, n}, *q = p;
    for (unsigned i = 0; i < 4 + n; i++) {
        while (GET32(UART_FR) & FR_TXFF)
            ;
        PUT32(UART_DR, i < 4 ? head[i] : q[i - 4]);
    }
    while (recv_byte() != 4)                   /* skip bytes before the 04 */
        ;
    uint8_t e[6], *r = ret;
    for (unsigned i = 0; i < 6; i++)
        e[i] = recv_byte();
    for (unsigned i = 0; i < nret; i++)
        r[i] = recv_byte();
    if (e[0] != 0x0e || e[1] != 4 + nret || (e[3] | e[4] << 8) != op || e[5])
        panic("HCI %x: bad answer %x %x %x %x %x %x",
              op, e[0], e[1], e[2], e[3], e[4], e[5]);
}

/* store v at p, low byte first, as HCI does */
static void le32(uint8_t *p, unsigned v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = v >> 8 * i;
}

/* Broadcom's vendor commands, each starting with a 4-byte address.
 *
 * Write_RAM's parameters are one contiguous block: the address, then the
 * bytes to store. HCI's length field is one byte, so a command carries at
 * most 255 parameter bytes (BT Part E 5.4.1): the address plus up to 251
 * data bytes. The bytes are read back with Read_RAM, so a write that did
 * not land panics here. */
void bt_write_ram(unsigned addr, const void *data, unsigned n) {
    uint8_t p[255], back[251];
    const uint8_t *d = data;
    assert(n <= sizeof back);             /* at most 251 data bytes      */
    le32(p, addr);                        /* bytes 0-3: the address      */
    for (unsigned i = 0; i < n; i++)      /* bytes 4 on: copy the data   */
        p[4 + i] = d[i];                  /*   in right after it         */
    hci(WRITE_RAM, p, 4 + n, 0, 0);

    bt_read_ram(addr, back, n);           /* read it back: did it land?  */
    for (unsigned i = 0; i < n; i++)
        if (back[i] != d[i])
            panic("Write_RAM to %x: byte %d reads %x, not %x",
                  addr, i, back[i], d[i]);
}
/* Launch_RAM's parameter is the address it calls */
void bt_launch_ram(unsigned addr) {
    uint8_t p[4];
    le32(p, addr);
    hci(LAUNCH_RAM, p, 4, 0, 0);
}
/* Read_RAM's parameters: bytes 0-3 the address, byte 4 the count */
void bt_read_ram(unsigned addr, void *data, unsigned n) {
    uint8_t p[5];
    le32(p, addr);
    p[4] = n;
    hci(READ_RAM, p, 5, data, n);
}

/* bt_run: run `code` on the Bluetooth CPU. Load it at LOAD, give it n
 * in the result area, call it (bit 0 of the address selects Thumb), and return
 * the word it stores after n. The answer slot starts as ~(3n+7), which
 * never equals 3n+7, so only the code's store can make it match. */
unsigned bt_run(const void *code, unsigned nbytes, unsigned n) {
    unsigned box[2] = { n, ~(3 * n + 7) }, answer;
    bt_write_ram(LOAD, code, nbytes);
    bt_write_ram(RESULT_AREA, box, sizeof box);
    bt_launch_ram(LOAD | 1);
    bt_read_ram(RESULT_AREA + 4, &answer, 4);
    return answer;
}
