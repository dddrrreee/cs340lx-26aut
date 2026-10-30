/* The PL011 UART (UART0): the registers and bits these programs use, and
 * pl011_up(), which sets it up.
 *
 * Where they come from:
 *   BCM  BCM2835 ARM Peripherals, chapter 13 (pp. 175-195): the PL011 as
 *        the Pi has it. Its register map (pp. 177-178) gives each address:
 *        bus address 0x7E201000 plus an offset (p. 177 prints the base as
 *        "0x7E20100"). The ARM sees bus address 0x7Ennnnnn at 0x20nnnnnn
 *        (p. 6), so UART0 is at 0x20201000.
 *   ARM  PrimeCell UART (PL011) Technical Reference Manual, DDI 0183G: the
 *        same offsets (Table 3-1, p. 3-3), and what each bit does
 *        (section 3.3). Both manuals describe every register; ARM's
 *        descriptions are the fuller ones. */
#ifndef PL011_H
#define PL011_H
#include "rpi.h"

enum {
    UART_DR   = 0x20201000,  /* data: send and receive (BCM p. 178; ARM p. 3-5)    */
    UART_FR   = 0x20201018,  /* flags (BCM p. 181; ARM p. 3-8)                      */
    UART_IBRD = 0x20201024,  /* divisor, integer part (BCM p. 183; ARM p. 3-9)      */
    UART_FBRD = 0x20201028,  /* divisor, 6-bit fraction (BCM p. 183; ARM p. 3-10)   */
    UART_LCRH = 0x2020102c,  /* frame format, FIFOs (BCM p. 184; ARM p. 3-12)       */
    UART_CR   = 0x20201030,  /* enables, loopback, RTS (BCM p. 185; ARM p. 3-15)    */
};

/* FR (ARM Table 3-4, p. 3-8) */
enum {
    FR_CTS  = 1 << 0,        /* the CTS input is low: the other end is ready */
    FR_RXFE = 1 << 4,        /* receive FIFO empty                           */
    FR_TXFF = 1 << 5,        /* transmit FIFO full                           */
};

/* LCR_H (ARM Table 3-10, p. 3-13). Its other bits stay 0: no parity, one
 * stop bit, no break. */
enum {
    LCRH_FEN    = 1 << 4,    /* FIFOs on     */
    LCRH_WLEN_8 = 3 << 5,    /* 8 data bits  */
};

/* CR (ARM Table 3-12, pp. 3-15 and 3-16) */
enum {
    CR_UARTEN = 1 << 0,      /* UART enable                                   */
    CR_LBE    = 1 << 7,      /* loopback enable: TX feeds RX inside the UART  */
    CR_TXE    = 1 << 8,      /* transmit enable                               */
    CR_RXE    = 1 << 9,      /* receive enable                                */
    CR_RTS    = 1 << 11,     /* 1 drives the RTS output low: "send to me"     */
};

/* UARTCLK, the PL011's reference clock: 48 MHz, set by the Pi's firmware
 * before our program starts. BCM p. 175 says only that the Clock Manager
 * controls it; the manual has no UART clock registers. */
enum { UARTCLK = 48000000 };

/* The baud divisor, kept in 64ths. The UART's internal clock ticks 16
 * times per bit (Baud16, ARM p. 2-5), so the divisor is UARTCLK / (16 x
 * baud) (ARM p. 3-10). FBRD has 6 bits, so it holds the fraction in 64ths:
 * IBRD takes div >> 6 and FBRD div & 63. Rounded to the nearest 64th. */
static inline unsigned pl011_div64(unsigned baud) {
    return (4 * UARTCLK + baud / 2) / baud;
}

/* pl011_up: run the PL011 at `baud`, 8 data bits, no parity, one stop bit
 * (8N1), FIFOs on. `cr` adds CR bits: CR_LBE for loopback, CR_RTS to talk
 * to the radio. */
static inline void pl011_up(unsigned baud, unsigned cr) {
    unsigned div = pl011_div64(baud);

    dev_barrier();                             /* other devices came before */
    PUT32(UART_CR, 0);                         /* 1. off: the next three must */
                                               /*    not change while it is on */
    PUT32(UART_IBRD, div >> 6);                /* 2. the divisor: whole part, */
    PUT32(UART_FBRD, div & 63);                /*    then 64ths               */
    PUT32(UART_LCRH, LCRH_WLEN_8 | LCRH_FEN);  /* 3. 8N1, FIFOs on; this write */
                                               /*    loads the divisor         */
    PUT32(UART_CR, CR_UARTEN | CR_TXE | CR_RXE | cr);  /* 4. on               */
    delay_ms(1);                               /* 5. workaround: let any stray */
    while (!(GET32(UART_FR) & FR_RXFE))        /*    word arrive, then throw   */
        GET32(UART_DR);                        /*    away everything received */
    dev_barrier();
}

#endif
