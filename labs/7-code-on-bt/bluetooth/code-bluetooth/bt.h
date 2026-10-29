/* The Bluetooth half of the BCM43430A1: its pins and the routines that
 * bring it up. */
#ifndef BT_H
#define BT_H

/* Pins. BT_REG_ON and WL_REG_ON are the chip's enables (CYW43438
 * datasheet section 20.1.1, p. 83). The PL011's pins to the radio are
 * GPIO30-33 at ALT3 (BCM2835 ARM Peripherals p. 102). */
enum {
    BT_REG_ON = 45,          /* low holds the Bluetooth half in reset */
    WL_REG_ON = 41,          /* the same for the Wi-Fi half           */
    CTS0 = 30,               /* in:  the radio's RTS                  */
    RTS0 = 31,               /* out: the radio's CTS                  */
    TXD0 = 32,               /* out: the radio's receive line         */
    RXD0 = 33,               /* in:  the radio's transmit line        */
};

/* HCI opcodes: Reset (Bluetooth Core 5.4, Vol 4 Part E sections 7.3 and 7.3.2),
 * then Broadcom's vendor commands to write, call and read the radio's RAM */
enum {
    HCI_RESET  = 0x0c03,
    WRITE_RAM  = 0xfc4c,
    READ_RAM   = 0xfc4d,
    LAUNCH_RAM = 0xfc4e,
};

/* Where chip.S goes in the radio's RAM, and its result area: n, then 3n+7 */
enum { LOAD = 0x210000, RESULT_AREA = 0x21ff00 };

/* Each routine panics if the radio does not answer as it should. */
void bt_reset(unsigned hold_ms);
unsigned bt_connect(unsigned timeout_us);
void hci(unsigned op, const void *p, unsigned n, void *ret, unsigned nret);
void bt_write_ram(unsigned addr, const void *data, unsigned n);
void bt_launch_ram(unsigned addr);
void bt_read_ram(unsigned addr, void *data, unsigned n);
unsigned bt_run(const void *code, unsigned nbytes, unsigned n);

#endif
