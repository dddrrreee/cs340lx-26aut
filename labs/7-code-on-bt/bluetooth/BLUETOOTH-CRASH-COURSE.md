# Pi Zero W radio bring-up: Bluetooth

## The big picture

The Pi Zero W's radio is one chip with two halves, Bluetooth and Wi-Fi, and each half has its own Cortex-M3 CPU (CYW43438
datasheet, pp. 38 and 46). The goal is to run our own code on both of those CPUs, starting from nothing but the Pi. We load no
Broadcom firmware: these programs talk only to the ROM that the Bluetooth half boots from. Broadcom calls the chip
the BCM43430A1. Its public datasheet is Cypress's CYW43438, which this tutorial calls CYW.

The Pi reaches each half by a different path:

| Half | Enable pin | Link to the Pi | What answers at power-on |
|---|---|---|---|
| Bluetooth | BT_REG_ON (GPIO45) | the Pi's PL011 UART, on GPIO30–33 | a ROM that speaks HCI |
| Wi-Fi | WL_REG_ON (GPIO41) | the Pi's Arasan SD host controller, on GPIO34–39 | an SDIO interface; no firmware runs |

```text
             +-- PL011 UART (GPIO30-33) -----> Bluetooth half: Cortex-M3 + ROM (HCI)
 Pi ARM1176 -+
             +-- Arasan SD host (GPIO34-39) -> Wi-Fi half: SDIO -> backplane -> Cortex-M3
```

So bringing up either half takes the same steps: turn the half on, open its link, and then use the link to load and
start a program on its CPU. This crash course does this for Bluetooth with one driver, `1-bt-bringup.c`, and each call in it is
one section below. A separate card, `BLUETOOTH-HELLO-WORLD.md`, then runs a C program of our own on the Bluetooth CPU,
which sends a line of text back to the Pi. It is all real code: the version printed here ran on a Pi Zero W.

<!-- source:start code-bluetooth/1-bt-bringup.c#notmain -->

```c
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
```

<!-- source:end -->

A Wi-Fi crash course will follow in the same form.

Each routine takes its settings as parameters and panics if anything goes wrong, so a run either prints its results
or stops at the first failure. `pl011_up`, `bt_reset` and `bt_run` each have a test program that runs them with other settings. The tests of
`bt_reset` and `bt_run` also have a control: a run whose check must come out the other way, so a check that always
passes would show. `bt_connect` is checked by the radio's answer to HCI Reset, in the driver. `chip[]`, which the
driver hands to `bt_run`, is the radio's program, `chip.S`, as bytes (section 4).

| File | What it holds |
|---|---|
| `pl011.h` | the PL011's registers, and `pl011_up(baud, cr)` |
| `bt.h`, `bt.c` | the radio's pins; `bt_reset`, `bt_connect` and `bt_run`; `hci()` and Broadcom's three RAM commands |
| `chip.S` | the 6-instruction program the Bluetooth CPU runs; `make` turns it into `chip_image.h` |
| `1-bt-bringup.c` | the driver |
| `0-test-pl011.c` | tests `pl011_up` alone, in loopback, at five rates |
| `2-test-bt-reset.c` | tests `bt_reset`: does the radio restart, and how long does it take? |
| `3-test-bt-run.c` | tests `bt_run`'s check: without the launch, the answer must stay wrong |
| `start.S`, `hello.ld`, `hello.c` | the hello world for the Bluetooth CPU, in C (the card `BLUETOOTH-HELLO-WORLD.md`); `make` turns it into `hello_image.h` |
| `4-bt-hello.c` | loads and runs the hello world, and prints what it sends back |

**Build and run.** Use an original Pi Zero W (not a Zero 2 W) with the class SD card, toolchain and bootloader, and
set `CS340LX_2026_PATH` to your class checkout: the Makefile builds against its libpi. In `code-bluetooth/`, `make`
builds every program, and `my-install 1-bt-bringup.bin` runs one. A failure prints a PANIC line and reboots the Pi. Success prints TRACE lines, with the values that change from run to run in brackets.
The `.out` files hold the expected output without the brackets: `make check` runs each program and compares its
output with them, and all five matched [Pi]. With more than one Pi attached, name yours:
`make check TTYUSB=/dev/...`. After changing what a program prints, `CAN_EMIT=1 make emit` regenerates its `.out` file.

**Names.** The Raspberry Pi's Bluetooth patch file for this chip is `BCM43430A1.hcd`. We never send it; we only
parsed it, to confirm which commands Broadcom's patch file uses (section 4, note 1). The chip ID that the Wi-Fi half
reports holds 43430 (0xa9a6) in its low 16 bits [Pi: the lab's wifi-id program]. Linux's device tree calls the part bcm43438
(`bcm283x-rpi-wifi-bt.dtsi`), and Cypress's datasheet for it is the CYW43438.

## Where everything comes from

A page pointer such as "(ARM p. 3-14)" names a document and a page: here, page 3-14 of ARM's PL011 manual, whose
copy is `docs/pl011-uart.pdf`. [Pi] means we measured it on a Pi Zero W; the
Evidence section at the end lists the runs. [derived] marks arithmetic, and [disassembly] a reading of the ROM's code. Every document has a copy in `docs/`, and `docs/SOURCES.md` says where
each copy came from.

| Name | Document | Copy |
|---|---|---|
| ARM | ARM PrimeCell UART (PL011) Technical Reference Manual, DDI 0183G | `docs/pl011-uart.pdf` |
| BCM | Broadcom, BCM2835 ARM Peripherals | `docs/bcm2835-peripherals.pdf` |
| CYW | Cypress CYW43438 datasheet, 002-14796 Rev. *O (printed page numbers; the PDF page is one higher) | `docs/cyw43438.pdf` |
| BT | Bluetooth Core Specification 5.4, Vol 4 Parts A and E | `docs/fetched/bt54-*.html` |
| ARMv7-M | ARMv7-M Architecture Reference Manual, DDI 0403E.e | `docs/armv7-m.pdf` |
| RPi docs | Raspberry Pi documentation, legacy config.txt options | `docs/fetched/rpi-legacy-config-txt-boot.adoc` |
| Linux | the Zero W's device tree, and the btbcm Bluetooth driver | `docs/fetched/` |
| Assigned Numbers | Bluetooth's list of company identifiers | `docs/fetched/assigned-numbers/` |
| PL081 | ARM PrimeCell Single Master DMA Controller (PL081) TRM, its errata, and the PL080 TRM | `docs/pl081-dma.pdf`, `docs/pl081-errata.pdf`, `docs/pl080-dma.pdf` |

A mark like [Pi: lab card 21] names one of the radio lab's earlier programs, its "lab cards", which measured the
fact. They are kept in the lab's repository (`labs/radio-processors/code/NN-name/`), not in this one; Evidence and
Details give the exact records. (The hello-world card is different: it is in this repository.)

To read further:

1. ARM chapter 2 explains how the UART works: sampling (p. 2-12), modem lines (p. 2-16), flow control (p. 2-17).
2. BT Part E chapter 7 lists every standard HCI command and event.
3. `docs/cortex-m3.pdf` is the M3's own manual.
4. InternalBlue's `hci.py`, copied as `docs/fetched/internalblue-hci.py`, lists Broadcom's other vendor commands.


## 1. Set up the PL011 UART: pl011_up(baud, cr)

**Goal.** Run the PL011 at a chosen rate with 8 data bits, no parity and one stop bit (8N1), and prove the rate is
right before any radio is involved.

**The idea.** A UART sends each byte as a frame of 10 bits: a start bit, 8 data bits and a stop bit. A UART's bit rate comes from dividing a reference clock, so setting one up means choosing that divisor and the frame format, then
turning the UART on. The PL011 is ARM's UART design, and the BCM2835 has one (BCM p. 175), which Broadcom calls UART0 (BCM p. 4). The class
console uses the Pi's other UART, the mini UART, so the PL011 is free for the radio.
Nothing in this section is specific to Bluetooth: `pl011_up` is a standard PL011 driver, and works the same
wherever the PL011 is routed. Bluetooth enters only through the driver's
CR_RTS argument and section 3, which routes the radio's four pins. The text uses ARM's register names; `pl011.h`
prefixes them, so CR is UART_CR, LCR_H is UART_LCRH, and each register's bits carry its name, as in CR_RTS and
LCRH_FEN.

**The code.** `pl011.h` names each register and bit we use. Each address comes from Broadcom's register map, each
bit from ARM's tables, and the comments give both manuals' pages. The file ends with the routine:

<!-- source:start code-bluetooth/pl011.h -->

```c
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
```

<!-- source:end -->

**Why each step.**

1. **Stop the UART first (CR = 0), because its rate (IBRD, FBRD) and format (LCR_H) must not change while it runs**
   (ARM p. 3-14). At entry the UART is running: CR reads 0x301: CR_UARTEN | CR_TXE | CR_RXE (bits 0, 8 and 9) [Pi].
2. **Compute the divisor (IBRD and FBRD) from the clock (UARTCLK, 48 MHz).** The UART's internal clock ticks 16 times per bit (Baud16, ARM p. 2-5), so
   the divisor is UARTCLK / (16 × baud) (ARM p. 3-10). UARTCLK is 48 MHz on the Pi: the firmware sets it (RPi docs,
   `init_uart_clock`).
   - For 115200 baud that is 48,000,000 / (16 × 115,200) = 26.042.
   - IBRD takes the whole part, 26. FBRD is a 6-bit field (ARM Table 3-7, p. 3-10), so it holds the fraction in
     64ths: 0.042 × 64 = 2.7, which rounds to 3.
   - The result is 115,177 baud, 0.02% slow, well inside what the radio tolerates (section 3, note 2). `pl011_div64()` does
     all of this in one integer division.
3. **Write the format (LCR_H) last, because that write is what loads the divisor (IBRD and FBRD).** The PL011 keeps IBRD and FBRD in holding
   registers and copies them to its baud generator only when LCR_H is written (ARM p. 3-14). In the other order, the new divisor would not take effect until some later LCR_H write. Broadcom's
   manual leaves this rule out, which is a bug in BCM; ARM's own manual for the PL011 has it (note 3). LCRH_WLEN_8 | LCRH_FEN (0x60 | 0x10 = 0x70)
   sets 8 data bits (WLEN, bits 6:5) and turns on the FIFOs (FEN, bit 4); LCR_H's other bits stay 0, for no parity and one stop bit (ARM Table 3-10,
   p. 3-13).
4. **Turn the UART on (CR), with any extra control bits the caller passes in `pl011_up`'s second argument,
   `cr`** (ARM pp. 3-15 and 3-16). The write is UART_CR = CR_UARTEN | CR_TXE | CR_RXE | `cr`: CR_UARTEN turns the UART on,
   CR_TXE and CR_RXE its transmitter and receiver, and `cr` adds one more bit. Callers pass one of two bits:
   - the loopback test at the end of this section, `0-test-pl011`, passes CR_LBE, loopback enable (bit 7). The
     receiver then reads the transmitter inside the UART, so every byte sent comes straight back with no wire;
   - the driver passes CR_RTS (bit 11), which asserts the RTS output and tells the radio it may send (section 3).
5. **Wait 1 ms, then empty the receive FIFO. This is a workaround.** The PL011's receiver can hold a stray word when the UART is turned on, so wait long enough for any such word to
   arrive, then throw everything away. 1 ms is about 12 frame times at
   115200, and the stray word arrived within one frame. Throwing everything away is safe because nothing real can
   have arrived yet. Without the wait, an earlier loopback program, the lab's pl011-up, read its bytes one behind on
   two other boards (note 4).
6. **Put a barrier (`dev_barrier()`) before and after,** because reads from two different peripherals can return out of order (BCM
   p. 7), and `pl011_up`'s UART accesses sit between the caller's GPIO and timer accesses.

**Notes.**

1. **Step 1 (stop the UART): ARM's full sequence.** ARM's full reprogramming sequence (p. 3-16) also waits for the current character to finish and flushes the transmit
   FIFO. Neither is needed here: nothing is being sent at entry (FR.BUSY is 0), and removing the flush changed
   nothing [Pi].
2. **Step 2 (compute the divisor): where the 48 MHz clock comes from.** Broadcom says only that "the Clock Manager" drives UARTCLK (BCM p. 175). The Raspberry Pi
   documentation gives the default, 48 MHz, for UART0 only (`init_uart_clock`). The frame times below confirm 48 MHz.
3. **Step 3 (write LCR_H last): a bug in Broadcom's manual, and how to find bugs like it.** ARM's rule, that IBRD, FBRD and LCR_H act
   as one register updated by the LCR_H write (p. 3-14), is missing from Broadcom's copy of the chapter (BCM
   pp. 183–184).
   - The trick that finds this: work out what the hardware block really is, then read its original datasheet.
     Broadcom's chapter says the block is ARM's PL011, and points to ARM's manual for it, revision r1p5 (BCM
     p. 175). ARM's manual has the rule.
   - The missing rule bit CS340LX last year: code that wrote LCR_H first sometimes worked anyway. It seemed to depend on whether
     the writes fell in the same prefetch-buffer block; that was never pinned down.
   - Two measured facts show how flipped code can look right. At entry the UART already runs at 115200 (IBRD 26,
     FBRD 3), so flipped code that sets 115200 changes nothing [Pi]. And on the Pi a new FBRD takes effect at
     once; only a change to IBRD exposes the wrong order [Pi: the lab's PL011 measurements]. Writing LCR_H last
     is right either way.
4. **Step 5 (wait 1 ms, then empty the receive FIFO): where the stray word comes from.** On our board the stray word is a break word, DR = 0x500: a 0 byte with the break and framing-error
   flags set.
   - The cause: with no GPIO routed to the PL011's receive input, the input reads low. A line held low for longer
     than a frame is a break, and the UART loads one 0 byte for it (ARM p. 3-6).
   - In four cold starts (`x-stray-word.c`, sessions 1, 3, 4 and 7), a break word was already in the receive FIFO at
     entry. Another arrived 83–84 µs, one frame, after an enable
     without loopback. With loopback, or with the input on GPIO15, which the USB-serial adapter holds high, none came
     [Pi].
   - An earlier pl011-up, the lab's, emptied the FIFO at once. The stray word then arrived just after, and 63 of the
     64 test bytes read one behind, on two other boards [Pi]. On one of them the stray word was 0xff, 78–79 µs after
     the enable, and its cause is not known [Pi].

**Where the PL011's pins can go** (BCM p. 102). The radio needs only GPIO30–33, but What to do next, item 3, uses
GPIO14 and GPIO15:

| GPIO | Function | Signals | On the Zero W |
|---|---|---|---|
| 14, 15 | ALT0 | TXD0, RXD0 | the class console's two pins |
| 30–33 | ALT3 | CTS0, RTS0, TXD0, RXD0 | the Bluetooth radio (section 3) |
| 36–39 | ALT2 | TXD0, RXD0, RTS0, CTS0 | taken by the Wi-Fi's SDIO |

Routing the PL011 to GPIO14 and GPIO15 (ALT0) puts the PL011 on the class console's two pins. The USB-serial adapter already
wired there then receives the PL011's bytes, and a logic analyzer on GPIO14 shows their bit timing. While routed, the
console's mini UART loses those pins, so the program has no console. At entry both pins are at ALT5, the mini UART:
GPFSEL1 = 0x12000 [Pi].

**How we know it works: `0-test-pl011.c`.** In loopback every byte we send must come back, and the time the bytes take must match the rate we set. The test runs `pl011_up` in loopback at five rates and sends 64 bytes at each. The test panics unless both of these hold:

1. **Every byte comes back unchanged,** with DR's four error flags (bits 11:8) clear. This checks the data path and the framing.
2. **The bytes take the predicted time.** From the first byte back to the last there are 63 frames of 10 bits, so at
   the divisor's rate they take 63 × 10 × 16 × (IBRD + FBRD/64) / UARTCLK [derived]. The measured time must match to
   within 3 µs: each end is timed with the Pi's 1 MHz timer, and the loop polls. This measures the real bit rate,
   which reading the registers back cannot.

The UART already runs at 115200 when the program starts, so the 115200 row alone proves nothing: every write could
have been ignored. The other rows need their own divisors. The 115385 row is UARTCLK / (16 × 26), the rate with
FBRD = 0. That rate differs from 115200 only in FBRD, and its 63-frame time moves by the predicted 10 µs.

| Rate (baud) | IBRD | FBRD | Predicted (µs) | Measured, six sessions (µs) |
|---|---|---|---|---|
| 115200 | 26 | 3 | 5,470 | 5,470, 5,470, 5,471, 5,470, 5,469, 5,469 |
| 115385 | 26 | 0 | 5,460 | 5,460, 5,458, 5,460, 5,460, 5,460, 5,461 |
| 57600 | 52 | 5 | 10,936 | 10,937, 10,936, 10,937, 10,937, 10,936, 10,937 |
| 230400 | 13 | 1 | 2,733 | 2,733, 2,733, 2,732, 2,733, 2,734, 2,733 |
| 921600 | 3 | 16 | 683 | 683, 682, 682, 681, 681, 682 |

All 64 bytes came back intact in every row [Pi]. At 3,000,000 baud (IBRD 1 and FBRD 0: UARTCLK/16, the fastest the
PL011 runs, BCM p. 175), all 64 bytes came back, but 63 frames took 214 µs against 210 predicted, in two sessions
[Pi]. The 4 µs excess is not explained, so 3,000,000 baud is left out of the test.

<!-- source:start code-bluetooth/0-test-pl011.c -->

```c
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
```

<!-- source:end -->

## 2. Turn on the Bluetooth half: bt_reset(hold_ms)

**Goal.** Restart the Bluetooth half, so its ROM boots from scratch and every run starts the same way.

**What it does.** `bt_reset` turns the Bluetooth half off and on again.
- BT_REG_ON (GPIO45) is the Bluetooth half's enable: low holds Bluetooth in reset, and high lets it boot.
- WL_REG_ON (GPIO41) is the Wi-Fi half's enable. It stays high throughout; no code here touches it.

The chip's power regulators stay on while either enable is high, so `bt_reset` is not a power cycle. The chip stays
powered, only Bluetooth restarts, and Wi-Fi is untouched (CYW p. 83). The GPIO numbers are from Linux's Zero W device
tree.

<!-- source:start code-bluetooth/bt.c#bt_reset -->

```c
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
```

<!-- source:end -->

**Why these delays.**

1. **The hold time is ours to choose, because the datasheet gives none for this case.** The datasheet's 10 ms rule is for when both enables were low, and its 150 ms rule is a power-up wait before SDIO (CYW p. 83). A 1 ms hold already restarts
   the ROM [Pi], and the driver uses 10 ms, a ten-times margin.
2. **Nothing waits after the release, because the radio says when it is ready.** `bt_connect` waits for the radio's RTS (the Pi reads it as FR_CTS; section 3) instead, which comes about 34 ms after the release [Pi].

**Notes.**

1. **The old delays.** Older programs held BT_REG_ON low for 50 ms and then waited 150 ms. Neither is needed [Pi].
2. **The 34 ms.** When our RTS is asserted after the radio has started, the radio asserts its RTS 5.3–5.6 ms later [Pi:
   lab card 21, two boards]. Here our RTS is asserted before the release, so the 34 ms is the radio's start-up time
   plus T4 (section 3, note 1).
3. **Where the pin numbers come from.** The device-tree lines are `shutdown-gpios = <&gpio 45 GPIO_ACTIVE_HIGH>` and
   `reset-gpios = <&gpio 41 GPIO_ACTIVE_LOW>` (`bcm2835-rpi-zero-w.dts`, lines 30 and 138). The Zero W's schematic
   is not public.
4. Holds shorter than 1 ms were not tried.

**How we know it restarted: `2-test-bt-reset.c`.** A restarted radio must look restarted in two independent ways, and
a radio we did not reset must look the opposite. Each pass of the test marks the radio's RAM, resets the radio or
not, and checks both ways. It borrows two tools from later sections: `bt_connect` (section 3) to reach the ROM,
and `bt_write_ram` and `bt_read_ram` (section 4) to mark and read the radio's RAM.

1. **Time.** Our RTS stays asserted, so the radio asserts its RTS as soon as its ROM is up: about 34 ms after a
   restart, and at once if it never stopped.
2. **Memory.** A word written into the radio's RAM at LOAD (0x210000, where section 4 loads its program) before the reset reads back as 0 afterwards,
   because the ROM zeroes that RAM when it starts. HCI Reset also clears the word, so the test reads the word before sending HCI Reset.
3. **The control, a hold of 0, with no reset.** The radio's RTS must come at once, and the word must survive.

| Hold | Radio's RTS after (µs) | Word after the hold | Word after HCI Reset |
|---|---|---|---|
| 0, the control | 1–2 | kept | 0 |
| 1 ms | 34,188–34,252 | 0 | 0 |
| 10 ms | 34,186–34,248 | 0 | 0 |
| 50 ms | 34,187–34,247 | 0 | 0 |

These are sessions 2–7, and every row behaved as expected [Pi]. So the ROM clears LOAD both when it starts and on HCI
Reset, and section 4 loads `chip.S` at LOAD after the Reset.

<!-- source:start code-bluetooth/2-test-bt-reset.c -->

```c
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
```

<!-- source:end -->

## 3. Connect to the Bluetooth ROM: bt_connect(timeout_us)

**Goal.** Connect the PL011 to the radio, wait until the radio says it is ready, and get its answer to the first
command.

**The idea.** The radio's UART is wired to the Pi's PL011 with four lines: two for data, and two for flow control,
RTS and CTS, with which each side tells the other it may send. The radio's ROM will not talk until it sees our RTS
and has asserted its own. So connecting means routing the four pins, CTS0, RTS0, TXD0 and RXD0 (GPIO30–33), waiting for
the radio's RTS, and only then sending the first HCI command, Reset, through `hci()`, which this section also builds.

**The four wires.** BCM p. 102 gives the GPIOs' ALT3 functions, and BCM p. 176 the PL011's inputs and outputs. CYW
Table 17 (p. 54) names the radio's pins and their directions. The Zero W's device tree marks GPIO30–33 "Used by BT
module" (`bcm2835-rpi-zero-w.dts`, line 72).

| GPIO (ALT3) | PL011 signal | Direction | Radio pin | Meaning |
|---|---|---|---|---|
| 30 | CTS0 | in | BT_UART_RTS_N | the radio can take bytes |
| 31 | RTS0 | out | BT_UART_CTS_N | the Pi can take bytes |
| 32 | TXD0 | out | BT_UART_RXD | data, Pi to radio |
| 33 | RXD0 | in | BT_UART_TXD | data, radio to Pi |

Both control lines are active low:

1. **Our RTS.** Setting CR_RTS (bit 11) drives our RTS output, RTS0 (GPIO31), low, which asserts our RTS and tells
   the radio it may send (ARM p. 3-15). With CR_RTS set, UART_CR = CR_UARTEN | CR_TXE | CR_RXE | CR_RTS
   (0x001 | 0x100 | 0x200 | 0x800 = 0xb01).
2. **The radio's RTS** arrives on our CTS input, CTS0 (GPIO30). FR_CTS (bit 0) reads 1 when that input is low, meaning
   the radio is ready (ARM p. 3-8).

CYW's start-up diagram shows the same handshake; note 1 draws it with our times.

<!-- source:start code-bluetooth/bt.c#bt_connect -->

```c
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
```

<!-- source:end -->

**Why each step.**

1. **Route CTS0, RTS0, TXD0 and RXD0 (GPIO30–33) to the PL011 (ALT3).** No pull-up is needed, because the radio drives
   its RTS itself (CYW p. 57).
2. **Wait for the radio's RTS, but only after routing,** because before routing FR.CTS already reads 1, and the wait
   would pass at once [Pi]. `bt_connect` panics after `timeout_us`; the driver allows 500 ms, about 15 times the
   34 ms start-up.
3. **Send HCI Reset only after the radio's RTS,** because a Reset sent earlier is lost [Pi: lab card 21].

**The HCI packets.** HCI, the host controller interface, is Bluetooth's standard interface between a host and its
radio. Over a UART, each packet starts with a type byte: 01 for a command, 04 for an event (BT Part A, Table 2.1).
- A command is a 16-bit opcode, sent low byte first, then a parameter length and the parameters (BT Part E 5.2 and 5.4.1).
- The opcode's top 6 bits are the group, OGF, and its low 10 bits the command, OCF. Reset is group 0x03 and command
  0x003, so Reset's opcode, `HCI_RESET` in `bt.h`, is 0x03 << 10 | 0x003 = 0x0c03 (BT Part E 7.3 and 7.3.2).
- The radio answers with a Command Complete event (BT Part E 7.7.14):

| Direction | Bytes | Fields |
|---|---|---|
| Pi to radio | `01 03 0c 00` | `01` command; `03 0c` opcode 0x0c03, Reset; `00` no parameters |
| radio to Pi | `04 0e 04 01 03 0c 00` | `04` event; `0e` Command Complete; `04` four bytes follow; `01` the radio will take one more command; `03 0c` the opcode it completes; `00` success |

`hci()` sends one command and panics unless the radio's answer completes that command with status 0. It reads each
byte with `recv_byte()`, which waits up to 1 s:

<!-- source:start code-bluetooth/bt.c#recv_byte..hci -->

```c
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
```

<!-- source:end -->

**How we know it works.** The radio decoded our 4 bytes, and we decoded its 7, at 115200 in both directions. So both
data lines and the rate are right, and the handshake completed. In the driver, the radio's RTS came 34,180–34,253 µs
after the release, in seven sessions [Pi].

**Notes.**

1. **CYW's start-up diagram, with our times.** It shows the same handshake: the host lowers the radio's CTS, and the radio then lowers its RTS to
   say it is ready (CYW Figure 19, p. 35). The datasheet names that delay T4 but gives no value, and says T4 assumes the
   radio has already finished starting up. Our measurements fill it in. Low means asserted; our RTS, RTS0, is the radio's
   CTS (BT_UART_CTS_N), and the radio's RTS (BT_UART_RTS_N) is our CTS, CTS0:

   ```text
   The radio has already started; then we assert our RTS [Pi: lab card 21]:

   BT_REG_ON   ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
   our RTS     ‾‾‾‾‾‾‾‾‾‾‾‾‾‾\_________________________________________
   radio's RTS ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾\______________________
                             |<--- T4: 5.3-5.6 ms --->|

   Our driver: we assert our RTS first, then release BT_REG_ON (sections 1-3):

   BT_REG_ON   _______/‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
   our RTS     \_______________________________________________________
   radio's RTS ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾\______________
                      |<--- start-up + T4: about 34 ms --->|
   ```
2. **The rate.** The radio's default rate is 115200 (CYW p. 44). The radio's own divisor turns 115200 into 115,385 baud, 0.16%
   fast, while ours is 0.02% slow. CYW allows the two ends ±2% between them (p. 44).
3. **Hardware flow control.** Setting CR.CTSEN (bit 15) makes the PL011 wait for CTS in hardware, which also prevents
   the lost Reset [Pi: lab card 21]. `bt_connect` waits in software so the handshake can be timed.

## 4. Run code on the Bluetooth CPU: bt_run(code, nbytes, n)

**Goal.** Load a program into the radio's RAM, run it on the Bluetooth CPU, and read back its answer.

**The idea.** Besides the standard HCI commands, the ROM answers three Broadcom commands. Write_RAM writes the radio's RAM, Launch_RAM calls an address in it, and Read_RAM reads it back. That is all it takes to run our own code: write the program into the radio's RAM, call it, and read what the program left behind. Here the program is `chip.S`: the Pi writes it at LOAD and n at RESULT_AREA, calls LOAD | 1, and reads 3n + 7 back from RESULT_AREA + 4.

**Broadcom's commands.** Broadcom does not document these commands; note 1 gives the public sources that do.

| Command | Opcode | Parameters | Returns |
|---|---|---|---|
| Write_RAM (`WRITE_RAM`) | 0xfc4c | a 4-byte address, low byte first, then the bytes | status |
| Read_RAM (`READ_RAM`) | 0xfc4d | a 4-byte address, then a 1-byte count | status, then the bytes |
| Launch_RAM (`LAUNCH_RAM`) | 0xfc4e | a 4-byte address, which it calls | status |

`bt_write_ram` reads back what it wrote and panics if the bytes did not land. A bad load therefore stops here, rather than
showing up later as a wrong answer.

<!-- source:start code-bluetooth/bt.c#le32..bt_read_ram -->

```c
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
```

<!-- source:end -->

**The program, and how it gets to the radio.** `chip.S` is six Thumb instructions for the radio's Cortex-M3: read n
from RESULT_AREA (0x21ff00), store 3n + 7 at RESULT_AREA + 4, and return. `chip.S` never runs on the Pi. `make` assembles `chip.S` for the M3, keeps the resulting 20 raw bytes, and writes them as a C array, `chip_image.h`, which `1-bt-bringup.c` and `3-test-bt-run.c` include as `chip[]`:

<!-- source:start code-bluetooth/Makefile:1-19 -->

```make
# x-stray-word.c (section 1, note 4) has no .out: make PROGS=x-stray-word.c
PROGS = 0-test-pl011.c 1-bt-bringup.c 2-test-bt-reset.c 3-test-bt-run.c 4-bt-hello.c
COMMON_SRC = bt.c
RUN ?= 0
CAN_EMIT ?= 0
BOOTLOADER = my-install
ifndef CS340LX_2026_PATH
$(error set CS340LX_2026_PATH to your class checkout)
endif
include $(CS340LX_2026_PATH)/libpi/mk/Makefile.robust-v3

# chip.S, the Bluetooth CPU's program: assemble it for the Cortex-M3 and take its bytes.
chip_image.h: chip.S
	arm-none-eabi-as -mcpu=cortex-m3 -mthumb $< -o chip.o
	arm-none-eabi-objcopy -O binary chip.o chip.bin
	xxd -i < chip.bin > $@
$(BUILD_DIR)/1-bt-bringup.o $(BUILD_DIR)/3-test-bt-run.o: chip_image.h
clean::
	rm -f chip.o chip.bin chip_image.h
```

<!-- source:end -->

The 20 bytes are 14 bytes of instructions, 2 bytes of padding and the 4-byte address RESULT_AREA (0x21ff00), low byte first. `chip_image.h` holds
them:

<!-- source:start code-bluetooth/chip_image.h -->

```c
  0x03, 0x49, 0x08, 0x68, 0x00, 0xeb, 0x40, 0x00, 0x07, 0x30, 0x48, 0x60,
  0x70, 0x47, 0x00, 0x00, 0x00, 0xff, 0x21, 0x00
```

<!-- source:end -->

<!-- source:start code-bluetooth/chip.S -->

```text
/* The program the radio's Bluetooth CPU runs. It never runs on the Pi.
 *
 * How it gets there: the Makefile assembles this file for the radio's
 * Cortex-M3 (arm-none-eabi-as -mcpu=cortex-m3 -mthumb), keeps only its
 * 20 raw bytes (objcopy -O binary), and writes them out as a C array
 * (xxd -i > chip_image.h). 1-bt-bringup.c includes that array as chip[],
 * and bt_run() sends it to the radio with Write_RAM at LOAD (0x210000),
 * then calls it with Launch_RAM at LOAD | 1 (0x210001; bit 0 = Thumb).
 *
 * What it does: the ROM calls it like a C function, so it may use r0-r3
 * and returns with bx lr. It reads n from the result area, RESULT_AREA in
 * bt.h (0x21ff00; keep the two equal), and writes 3n+7 into the next word. */
    .syntax unified         @ ARM's unified assembler syntax
    .cpu cortex-m3          @ the radio's CPU, not the Pi's ARM1176
    .thumb                  @ Thumb code: the only kind an M3 runs
    .thumb_func             @ mark _start as a Thumb function
    .global _start
_start:
    @ the result area: n, then the answer
    ldr   r1, =0x0021ff00
    ldr   r0, [r1, #0]
    @ 3n + 7
    add   r0, r0, r0, lsl #1
    adds  r0, r0, #7
    str   r0, [r1, #4]
    @ back to the ROM, which goes on serving HCI
    bx    lr
    .ltorg
```

<!-- source:end -->

<!-- source:start code-bluetooth/bt.c#bt_run -->

```c
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
```

<!-- source:end -->

`bt.h` names the pins, the HCI opcodes, `LOAD` and `RESULT_AREA`:

<!-- source:start code-bluetooth/bt.h:6-28 -->

```c
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
```

<!-- source:end -->

**Why each step.**

1. **Load at LOAD (0x210000), after the Reset.** The ROM clears LOAD when it starts and on HCI Reset (section 2),
   so a program loaded before the Reset would be wiped. `chip.S` never names this address: its code runs wherever it
   is loaded, and its one absolute address is the result area's. The lab's ROM study, not yet published, places this RAM in the
   ROM's zero-initialized data. Code and data at LOAD did not disturb the ROM while it served only HCI Reset and the three RAM commands [Pi]; the
   hello-world card maps the rest of the radio's RAM.
2. **Put n and the answer in the result area, RESULT_AREA (0x21ff00).** It is only a convention; any free RAM would
   do. The Pi writes n there with Write_RAM, `chip.S` reads it and stores 3n + 7
   in the next word, and after the program returns the Pi reads that word with Read_RAM. Both addresses are radio addresses, not Pi addresses.
3. **Launch at LOAD | 1 (0x210001),** because bit 0 of a branch target selects Thumb, the only instruction set an M3 runs. A
   target with bit 0 clear faults (ARMv7-M p. A2-31).
4. **Start the answer slot, box[1], at ~(3n + 7).** ~(3n + 7) never equals 3n + 7, so only the program's store can make it match.

**How we know it ran.**

1. **The answer.** The driver panics unless the answer is 3n + 7, for a new n each run. n is the Pi's microsecond
   timer times 2654435761, which is 0x9e3779b1, about 2^32 divided by the golden ratio. The multiplication spreads
   the timer's changing low bits across the whole word, so n differs completely from run to run. The check passed in seven sessions [Pi].
2. **The control, `3-test-bt-run.c`.** Without Launch_RAM, the answer slot keeps its starting value; after Launch_RAM, the slot holds 3n + 7 [Pi, four sessions].
3. **The ROM survives.** The ROM answers the Read_RAM after the program, so the ROM is still serving commands once the
   program returns.

<!-- source:start code-bluetooth/3-test-bt-run.c -->

```c
/* Test the driver's 3n+7 check: without Launch_RAM the answer must stay wrong.
 * The answer slot starts as ~(3n+7), so only the radio running chip.S can
 * make it 3n+7. With n = 0x55555553, 3n+7 = 0 (mod 2^32): an answer slot
 * that started at 0 would pass without running anything. */
#include "rpi.h"
#include "pl011.h"
#include "bt.h"

static const uint8_t chip[] = {
#include "chip_image.h"
};

void notmain(void) {
    pl011_up(115200, CR_RTS);
    bt_reset(10);
    bt_connect(500 * 1000);

    unsigned n = 0x55555553, box[2] = { n, ~(3 * n + 7) }, answer;
    bt_write_ram(LOAD, chip, sizeof chip);
    bt_write_ram(RESULT_AREA, box, sizeof box);
    bt_read_ram(RESULT_AREA + 4, &answer, 4);
    if (answer != ~(3 * n + 7))
        panic("without Launch_RAM the answer changed to %x", answer);
    trace("without Launch_RAM the answer stays ~(3n+7) [answer=%x]\n", answer);

    bt_launch_ram(LOAD | 1);
    bt_read_ram(RESULT_AREA + 4, &answer, 4);
    if (answer != 3 * n + 7)
        panic("after Launch_RAM the answer is %x, not 3n+7", answer);
    trace("after Launch_RAM it is 3n+7 [answer=%x]\n", answer);
}
```

<!-- source:end -->

**Notes.**

1. **Where the commands come from.**
   - InternalBlue, a research tool for Broadcom Bluetooth chips, defines all three opcodes (`hci.py`, lines 331–334).
   - The Pi's own patch file for this chip, `BCM43430A1.hcd`, is 120 Write_RAMs and one Launch_RAM. Linux's btbcm
     driver sends such a file unchanged (`btbcm.c`, lines 222–267).
   - The top six bits of all three opcodes are 0x3F, the group the Bluetooth spec reserves for vendor commands (BT
     Part E 5.4.1).
2. **The ROM's call.** The ROM calls the program with `blx r4` at 0x768cc, so the program returns with `bx lr`. The ROM queues Launch_RAM's Command Complete at 0x768c4, before the call, so that event does not prove the program ran
   [disassembly; lab card 00, lines 48 and 54].
3. **Why not start the slot at 0?** For n = 0x55555553, 3n + 7 = 0 (mod 2^32), so a slot that started at 0 would pass
   with nothing run. The control uses that n.

## What to do next

**First, a hello world in C.** The card `BLUETOOTH-HELLO-WORLD.md` writes a C program for the Bluetooth CPU that sends a line
of text to the Pi. It covers what C on the radio needs: a stack, the radio's memory map, an entry point, linking, the
build, a way to report back, and life without a C runtime. Its code is in `code-bluetooth/`, with the rest.

**Then, things to try.** Each is small and needs only what the crash course and the card show. Check each the same way: panic if it fails, and
print a value that only a working program could produce.

**On the Pi.**

1. **Push the PL011's rate.** Find the fastest rate `0-test-pl011` still passes, and look into section 1's
   unexplained 4 µs at 3,000,000 baud. The PL011 tops out at UARTCLK/16 (BCM p. 175), and the firmware sets UARTCLK
   (`init_uart_clock`, RPi docs), so a faster UARTCLK allows a faster PL011. The radio's UART goes up to 4 Mbaud
   (CYW p. 44).
2. **Find the shortest reset hold.** A hold of 1 ms restarts the radio and the control, a hold of 0, does not
   (section 2); nothing in between was tried. `bt_reset` counts in milliseconds, so this needs a microsecond version.
3. **Print through the PL011.** Route GPIO14 and GPIO15 to ALT0, where they carry the PL011's transmit and receive lines, BCM's TXD0 and RXD0 (BCM p. 102;
   not `bt.h`'s TXD0 and RXD0, which are GPIO32 and GPIO33), instead of ALT5, the mini UART that the console uses. Your terminal then shows the PL011's bytes.
   Route them back to ALT5 before using the console again.
4. **Use the PL011's interrupts instead of polling.** ARM's UARTIMSC, UARTRIS, UARTMIS and UARTICR registers mask,
   read and clear the PL011's interrupts (pp. 3-17 to 3-21). On the BCM2835 the PL011 is interrupt 57 (BCM p. 113).

**On the radio.**

5. **Change `chip.S`'s arithmetic** to 5n + 3, and the Pi side with it: the driver's check (`1-bt-bringup.c`),
   `bt_run`'s starting value ~(3n + 7) (`bt.c`), and `3-test-bt-run.c`, whose n works only for 3n + 7. Then
   `CAN_EMIT=1 make emit` regenerates the expected output.
6. **Print a number.** Put a value into the hello world's text, such as the M3's CPUID register at 0xE000ED00
   (ARMv7-M B3.2.3, p. B3-598). It should read 0x412fc231; the Wi-Fi CPU's reads 0x412fc230, an older revision
   [Pi: lab card 29]. C on the radio has no library, so this needs a small hex formatter.
7. **Ask the ROM about the chip** with standard HCI commands. Read_BD_ADDR, opcode 0x1009 (BT Part E 7.4.6), returns
   the radio's Bluetooth address. Linux's btbcm notes that a BCM43430A1 with no configured address reports
   AA:AA:AA:AA:AA:AA (`btbcm.c`, lines 28 and 122).
8. **Measure the stack.** The hello world runs on its own 2 KB stack, below `__stack_top` (0x21d800). Have the Pi
   fill that stack with a pattern before the launch (Write_RAM, 251 bytes at a time), and read it back afterwards:
   the lowest overwritten word shows how deep the program went.
9. **Clear the BSS in `start.S`,** so C statics start at zero. `hello.ld` then needs two symbols that mark where the
   BSS starts and ends.
10. **Send more events.** Send ten numbered events, and check on the Pi that all ten arrive, in order. The ROM has
    room for about 30 small events [Pi: lab card 27]; once none is left, its event routine (`ROM_SEND_EVENT`) uses a
    NULL buffer, so stay well below that.

**What else the radio can do.** The lab's earlier programs used the Bluetooth half's AES-128 engine, a supervisor
call (SVC) on its M3, the radio's RTS pad as an output the Pi reads on CTS0 (GPIO30), as FR_CTS, its DMA engine (a PL081) running a
finite automaton, `printf` built on the ROM's event routine, and one token passed through the Pi's ARM and both radio
CPUs.

## Evidence

**The board.** One Pi Zero W: revision 0x9000c1, firmware 0x61fd17f9, serial 0x00000000e00a7e78. It was attached to the lab's host, pi-1, and ran in ten sessions: six on 2026-10-27 and four on 2026-10-29. Each session began with a USB power cut, and the serial was
read before and after. The logs and the binaries that ran are kept in the lab's repository, not in this one.

| Session | What ran |
|---|---|
| 1 | `0-test-pl011` (writing one byte per pass, and trying 3 Mbaud); `1-bt-bringup`; `2-test-bt-reset` (reading the word after HCI Reset); `x-stray-word` |
| 2 | `0-test-pl011` (TX FIFO kept full; 3 Mbaud); `2-test-bt-reset` (reading the word before HCI Reset); `1-bt-bringup` |
| 3 | `x-stray-word`, `0-test-pl011`, `1-bt-bringup`, `3-test-bt-run`, `2-test-bt-reset` |
| 4 | `0-test-pl011` to `3-test-bt-run`, and `x-stray-word` |
| 5 | `2-test-bt-reset` and `1-bt-bringup` |
| 6 | an earlier draft's code: `0-test-pl011` to `3-test-bt-run` |
| 7 | the printed code, on 2026-10-29: `0-test-pl011` to `3-test-bt-run`, and `x-stray-word` |
| 8 | `4-bt-hello`, without its last check, the ROM's second answer |
| 9 | `4-bt-hello`, on the ROM's stack |
| 10 | `4-bt-hello` on its own stack, the printed code |

Sessions 1–5 ran earlier versions that returned error codes instead of panicking, and session 6 ran an earlier draft's code,
with `pl011_up` in its own file and no read-back in `bt_write_ram`. Their measuring code is the printed code's, except
where the table says otherwise. On 2026-10-29, `make check` also passed for all five programs, run with `my-install`. Built against the 2025 class runtime, the printed code gives the session 7 and 10 binaries byte for byte; this
copy builds against `CS340LX_2026_PATH`'s libpi, so its binaries differ.

**Every measured value.** The radio's RTS is timed from the release of BT_REG_ON. Loopback times are 63 frames,
from the first byte back to the last.

| Session | Loopback at 115200 / 115385 / 57600 / 230400 / 921600 baud (µs) | Driver: radio's RTS (µs) | Reset test: first connect; then holds of 0, 50, 10, 1, 0, 10, 1 ms (µs) |
|---|---|---|---|
| 1 | 5,470 / 5,460 / 10,937 / 2,733 / 683 | 34,226 | 34,236; 1, 34,204, 34,196, 34,195, 2, 34,203, 34,236 |
| 2 | 5,470 / 5,458 / 10,936 / 2,733 / 682 | 34,232 | 34,233; 1, 34,247, 34,196, 34,250, 2, 34,190, 34,209 |
| 3 | 5,471 / 5,460 / 10,937 / 2,732 / 682 | 34,253 | 34,229; 2, 34,188, 34,198, 34,245, 1, 34,234, 34,241 |
| 4 | 5,470 / 5,460 / 10,937 / 2,733 / 681 | 34,199 | 34,202; 1, 34,205, 34,198, 34,188, 1, 34,206, 34,235 |
| 5 | not run | 34,180 | 34,199; 1, 34,187, 34,186, 34,248, 1, 34,192, 34,232 |
| 6 | 5,469 / 5,460 / 10,936 / 2,734 / 681 | 34,237 | 33,792; 1, 34,199, 34,208, 34,241, 2, 34,233, 34,194 |
| 7 | 5,469 / 5,461 / 10,937 / 2,733 / 682 | 34,184 | 33,801; 1, 34,212, 34,248, 34,208, 1, 34,229, 34,252 |

So the radio's RTS came 33.8–34.3 ms after every release, and 1–2 µs after every hold of 0. The reset table in
section 2 uses sessions 2–7, the ones that read the RAM word before HCI Reset. The control in `3-test-bt-run` ran in
sessions 3, 4, 6 and 7: `0xffffffff` without Launch_RAM, then 0 after it, each time. `x-stray-word` ran in sessions
1, 3, 4 and 7, and 3 Mbaud in sessions 1 and 2 (214 µs both times).

The lab's earlier programs pl011-up and bt-run, and lab card 21, ran on two other boards. Their records are
`bringup/RUNS.md` and `bringup/EVIDENCE.md` in the lab's repository (release v1.41).

**Not yet done.** All of this is one board. The times and the 1 ms hold should be repeated on others.

## Details

The exact values, bits and sources behind the main text, by section.

### Documents

| Name | Document, and its copy | Used here | To read more |
|---|---|---|---|
| ARM | ARM PrimeCell UART (PL011) Technical Reference Manual, DDI 0183G: `docs/pl011-uart.pdf` | chapter 3, Programmers Model: the register map (Table 3-1, p. 3-3), each register's bits (section 3.3), the baud formula (p. 3-10), the LCR_H rule (p. 3-14), the reprogramming sequence (p. 3-16) | chapter 2: how the UART works (p. 2-4), modem lines (p. 2-16), hardware flow control (p. 2-17) |
| BCM | Broadcom, BCM2835 ARM Peripherals: `docs/bcm2835-peripherals.pdf` | addresses (p. 6), barriers (p. 7), the GPIO alternate functions (p. 102), the PL011 chapter 13 (pp. 175–195) | chapter 13 itself refers to ARM's manual "for the in-depth UART overview" (p. 175) |
| CYW | Cypress CYW43438 datasheet, 002-14796 Rev. *O: `docs/cyw43438.pdf` | the two CPUs (sections 9 and 11.1), the start-up handshake (Figure 19, p. 35), the radio's UART (section 10.2, p. 44), pin states (Table 20, p. 57), the REG_ON pins (section 20.1, p. 83) | page numbers are the printed ones; the PDF page is one higher |
| BT | Bluetooth Core Specification 5.4, Vol 4 Parts A and E: `docs/fetched/bt54-*.html`, from bluetooth.com | Part A Table 2.1 (packet types); Part E sections 5.2 (byte order), 5.4.1 and 5.4.4 (packets), 7.3 and 7.3.2 (Reset), 7.4 and 7.4.1 (Read_Local_Version_Information), 7.7.14 (Command Complete) | Part E chapter 7 lists every standard command and event |
| ARMv7-M | ARMv7-M Architecture Reference Manual, DDI 0403E.e: `docs/armv7-m.pdf` | the Thumb bit of a branch target (BLXWritePC, A2.3.1, p. A2-31) | the M3's own manual, `docs/cortex-m3.pdf` |
| Linux | `bcm2835-rpi-zero-w.dts`, `bcm283x-rpi-wifi-bt.dtsi` and `drivers/bluetooth/btbcm.c`: `docs/fetched/` | the enable pins, the part's name, how Broadcom patch files are sent | the rest of `btbcm.c`: Broadcom's other setup commands |
| RPi docs | Raspberry Pi documentation, legacy config.txt options: `docs/fetched/rpi-legacy-config-txt-boot.adoc` | `init_uart_clock`, the PL011's 48 MHz | the same page's other boot options |
| Patch | The Raspberry Pi's Bluetooth patch file, `BCM43430A1.hcd`: `docs/fetched/hcd/` | what Broadcom's own loader sends | — |
| IB | InternalBlue, a research tool for Broadcom Bluetooth chips, `hci.py`: `docs/fetched/internalblue-hci.py` | the RAM commands' opcodes (lines 331–334) | its other vendor commands |
| Assigned Numbers | Bluetooth SIG, company identifiers: `docs/fetched/assigned-numbers/company_identifiers.yaml` | 0x000f is Broadcom (line 12135) | the other Assigned Numbers files in the same repository |
| Lab cards | the radio lab's earlier programs, `labs/radio-processors/code/NN-name/` in the lab's repository | the crash course cites lab cards 00, 21, 27 and 29; the hello-world card cites more | each card's README, `EVIDENCE.md` and run records |
| PL081 | ARM PrimeCell Single Master DMA Controller (PL081) TRM, DDI 0218E: `docs/pl081-dma.pdf`, with its errata, `docs/pl081-errata.pdf` | the radio's DMA engine, in the hello-world card: two channels (p. 1-2) | `docs/pl080-dma.pdf`, ARM DDI 0196G, for the eight-channel PL080 |

`docs/SOURCES.md` gives each copy's origin, date and SHA-256. [derived] marks arithmetic. [disassembly] marks a reading
of the radio's ROM at the stated address: the lab read the ROM out with Read_RAM and disassembled it. The ROM is
only read, never changed.

### 1. pl011_up

1. ARM's no-change rule (step 1) has two cases: LCR_H, IBRD and FBRD must not change while the UART is enabled, nor while it finishes a
   character after being disabled (ARM p. 3-14; BCM p. 184 repeats this rule, though not note 3's).
2. The arithmetic: 48,000,000 / (16 × 115,200) = 26.042, and FBRD = 0.042 × 64 + 0.5 → 3, the rounding of ARM's
   Example 3-1 (p. 3-10). The rate is 48 MHz / (16 × (26 + 3/64)) = 115,177 baud [derived].
3. ARM: "The UARTLCR_H, UARTIBRD, and UARTFBRD registers form the single 30-bit wide UARTLCR Register that is updated
   on a single write strobe generated by a UARTLCR_H write" (p. 3-14). Broadcom's copy of the chapter leaves this out
   (BCM pp. 183–184). On the Pi, a new FBRD takes effect without an LCR_H write [Pi: the lab's PL011 measurements,
   its section 5, "Change the baud rate while the link is up"].
4. The bits: LCRH_WLEN_8 | LCRH_FEN = 0x70 sets WLEN = 0b11 (bits 6:5) and FEN (bit 4); PEN, STP2 and BRK stay 0 (ARM Table 3-10,
   p. 3-13). In CR, CR_UARTEN is bit 0, CR_LBE bit 7, CR_TXE bit 8, CR_RXE bit 9 and CR_RTS bit 11 (ARM Table 3-12, pp. 3-15 and 3-16). Sending
   needs UARTEN and TXE; receiving needs UARTEN and RXE (p. 3-16).
5. Loopback: "the UARTTXD path is fed through to the UARTRXD path" (ARM p. 3-15). In `0-test-pl011` no pin is routed
   to the PL011 at all: at entry GPIO14 and 15 are at ALT5 for the console (GPFSEL1 = 0x12000), and GPIO30–39 are
   inputs (GPFSEL3 = 0) [Pi].
6. The break: DR's BE is bit 10 and FE bit 8, and a break is the input held low "for longer than a full-word
   transmission time" (ARM Table 3-2, p. 3-6). In sessions 1, 3, 4 and 7, FR was 0x81 at entry with BE and FE set in RIS
   (0x2ad), and the FIFO's first word was 0x500. Enabling with no loopback and no pin routed brought another 0x500
   83–84 µs later. Loopback, or the input routed to GPIO15, brought none [Pi: `x-stray-word.c`]. The drain stops when
   FR.RXFE (bit 4) is 1.
7. Without the 1 ms wait, an earlier pl011-up drained too soon, and 63 of the 64 bytes read one behind, on two boards
   [Pi: the lab release's `bringup/RUNS.md`, "The stray receive word in pl011-up"].
8. On one of those two boards the word was 0xff, 78–79 µs after a loopback enable, in two runs, with the FIFO empty at entry
   (FR = 0x91) [Pi: the same record]. It is not explained.
9. The test's 3 µs window: each timestamp comes from the Pi's 1 MHz timer, so each end can be 1 µs off, and the loop
   polls.
10. At entry FR.BUSY (bit 3) is 0 [Pi]. At 3,000,000 baud, IBRD is 1 and FBRD 0, and UARTCLK/16 is the PL011's top
    rate (BCM p. 175).

### 2. bt_reset

1. CYW p. 83, on BT_REG_ON, in full: "When this pin is low and WL_REG_ON is high, the BT section is in reset." Its two timings:
   "at least a 10 ms time delay between consecutive toggles (where both signals have been driven low)", and "Wait at
   least 150 ms after VDDC and VDDIO are available before initiating SDIO accesses".
2. Lab card 21's 5.3–5.6 ms covers 24 boots on two boards. In it, our RTS was asserted after the radio had started.

### 3. bt_connect

1. UART_CR = CR_UARTEN | CR_TXE | CR_RXE | CR_RTS = 0x001 + 0x100 + 0x200 + 0x800 = 0xb01. The PL011's modem lines are active low:
   CR.RTS = 1 drives nUARTRTS low (ARM p. 3-15), and FR.CTS reads 1 when nUARTCTS is low (ARM p. 3-8).
2. CYW Figure 19 (p. 35) names the radio's lines BT_UART_CTS_N and BT_UART_RTS_N. Its delay T4 "assumes the BTH
   device has already completed initialization": lab card 21's 5.3–5.6 ms is that case, not the 34 ms start from reset.
3. Before routing, FR read 0x81 or 0x91 at entry: the CTS bit is already 1 [Pi].
4. HCI fields: the type byte is the "HCI packet indicator" (BT Part A, Table 2.1). In the reply, `01` is
   Num_HCI_Command_Packets and the final `00` is Status (BT Part E 7.7.14). The OGF takes the opcode's upper 6 bits
   and the OCF the other 10 (BT Part E 5.4.1); Reset is OGF 0x03 (BT Part E 7.3) and OCF 0x0003 (7.3.2).
5. The rates: CYW Table 13 (p. 44) turns 115200 into 115,385 baud on the radio; the Pi makes 115,177. They are 0.18%
   apart, inside CYW's combined ±2% [derived].
6. CR.CTSEN is bit 15 (ARM p. 3-15).

### 4. bt_run

1. InternalBlue's names are `VSC_Write_RAM = 0xFC4C`, `VSC_Read_RAM = 0xFC4D` and `VSC_Launch_RAM = 0xFC4E` (`hci.py`,
   lines 331–334). Linux's btbcm first sends Download_Minidriver, opcode 0xfc2e, and then each command of the patch
   file unchanged (`btbcm.c`, lines 222–267). BT Part E 5.4.1: "0x3F is reserved for vendor-specific debug commands".
2. `chip.S` assembles to 20 bytes: six Thumb instructions in 14 bytes, 2 bytes of padding, and the 4-byte
   address RESULT_AREA.
3. The ROM facts come from lab card 00, `labs/radio-processors/code/00-bt-hello/README.md` (lines 48 and 54), and the
   lab's ROM study: `blx r4` at 0x768cc, and the queued Command Complete at 0x768c4 [disassembly]. That LOAD = 0x210000
   lies in the ROM's zero-initialized data comes from the lab's ROM study, which is not published; that the ROM clears
   it is measured (section 2).
4. The control: for n = 0x55555553 the slot starts at ~(3n + 7) = 0xffffffff, and 3n + 7 = 0 (mod 2^32). In the
   driver, n is the Pi's microsecond timer times 2654435761.

## Check it yourself: what to read

Every claim above points to a page, and all the documents are in `docs/`. To check our reading, read these places,
in tutorial order. The code comments carry the same pointers.

**Section 1, `pl011_up`: ARM's PL011 manual (`docs/pl011-uart.pdf`) and Broadcom's (`docs/bcm2835-peripherals.pdf`).**

1. ARM p. 2-5: Baud16, the internal clock that ticks 16 times per bit.
2. ARM Table 3-1, p. 3-3: every register's offset. BCM pp. 177–178 has the same map with UART0's base; p. 177 prints
   that base wrong, as "0x7E20100"; BCM p. 6 maps bus addresses to the ARM's.
3. ARM p. 3-6 (Table 3-2): DR, its error flags, and what a break is.
4. ARM p. 3-8 (Table 3-4): FR, including CTS, RXFE and TXFF.
5. ARM pp. 3-9 and 3-10: IBRD and FBRD, the divisor formula and Example 3-1's rounding.
6. ARM pp. 3-12 to 3-14: LCR_H, its bits (Table 3-10), and on p. 3-14 the rule that the LCR_H write loads the divisor.
   Compare BCM pp. 183–184, which leaves that rule out.
7. ARM pp. 3-15 and 3-16 (Table 3-12): CR, including LBE, RTS and CTSEN, and the full reprogramming sequence.
8. BCM p. 7: why peripheral accesses need barriers. BCM p. 102: which GPIO carries which UART signal at each ALT
   function. BCM p. 175: the UART is ARM's PL011, ARM's r1p5 manual describes it, and the Clock Manager drives UARTCLK.
9. RPi docs (`docs/fetched/rpi-legacy-config-txt-boot.adoc`): `init_uart_clock`, the 48 MHz default.

**Section 2, `bt_reset`: the CYW43438 datasheet (`docs/cyw43438.pdf`) and Linux.**

1. CYW section 20.1, p. 83: BT_REG_ON and WL_REG_ON, the OR-ed regulators, and the 10 ms and 150 ms rules.
2. `docs/fetched/bcm2835-rpi-zero-w.dts`, lines 30 and 138: BT_REG_ON (GPIO45) and WL_REG_ON (GPIO41).

**Section 3, `bt_connect`: CYW, ARM and the Bluetooth spec.**

1. CYW Figure 19, p. 35: the start-up handshake and T4.
2. CYW section 10.2, p. 44: the radio's UART, its 115200 default, Table 13's rates and the ±2% rule.
3. CYW Table 20, p. 57: the radio's RTS is an output with no pull.
4. BT Vol 4 Part A, Table 2.1 (`docs/fetched/bt54-uart-transport.txt`): the 01 and 04 packet types.
5. BT Vol 4 Part E (`docs/fetched/bt54-hci-functional.txt`): 5.2 the byte order, 5.4.1 the command packet and the
   OGF/OCF split, 5.4.4 the event packet, 7.3 and 7.3.2 Reset, 7.7.14 Command Complete.

**Section 4, `bt_run`: Broadcom's vendor commands and the M3.**

1. BT Vol 4 Part E 5.4.1: OGF 0x3F is reserved for vendor commands.
2. `docs/fetched/internalblue-hci.py`, lines 331–334: the three opcodes.
3. `docs/fetched/btbcm.c`, lines 222–267: how Linux sends Broadcom's patch file; `docs/fetched/hcd/BCM43430A1.hcd` is
   that file.
4. ARMv7-M section A2.3.1, p. A2-31 (`docs/armv7-m.pdf`): BLXWritePC, the Thumb bit of a branch target.
5. CYW sections 9 and 11.1, pp. 38 and 46: the two Cortex-M3s.

**Finding a page.** CYW's PDF page is one more than the printed number. ARM and ARMv7-M number their pages by
chapter, as in "3-14" and "A2-31"; search for that string in the footers. BCM's printed and PDF pages match. To search
a PDF as text, run `pdftotext -layout FILE.pdf out.txt` and grep it.
