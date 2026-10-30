# Hello world on the Bluetooth CPU, in C

This card follows the Bluetooth crash course, `BLUETOOTH-CRASH-COURSE.md`, and assumes its sections 1–4 and its conventions (its
"Where everything comes from"): the marks [Pi], [derived] and [disassembly], and names such as CYW. "Section N"
below means the crash course's section N. Its code is in `code-bluetooth/`, with the crash course's.

**Goal.** Run a C program on the radio's Bluetooth CPU, and have it send a line of text to the Pi.

**What C on the radio needs.** Section 4's `chip.S` was six hand-written instructions that needed only a load
address. A C program needs seven things, each answered below:

1. a stack: our own, which `start.S` sets up;
2. places for code and data: the memory map;
3. an entry point that Launch_RAM can call: `start.S`;
4. to be linked where it is loaded: `hello.ld`;
5. a build for the M3: the Makefile;
6. a way to report back: the result area, or the ROM's event routine (`hello.c`);
7. to live without a C runtime: no BSS clearing, no C library, and at most 251 bytes per Write_RAM.

**What stays the same.** The Pi's side of section 4: bring the radio up, load the program at LOAD with Write_RAM,
and call LOAD | 1 with Launch_RAM. And the radio's UART: the ROM brings it up when the radio starts and runs it for HCI, so
our program never initializes it. It is Broadcom's own UART, not a PL011 (its data register is at 0x36001c), and our
text reaches it only through the ROM's event routine.

**1. A stack.** C needs a stack. Launch_RAM calls our code like a C function, inside the ROM's Launch_RAM handler
(section 4, note 2), on the ROM's process stack, with interrupts masked [Pi: lab card 04]. SP is near 0x21db58
there, and nobody knows how deep that stack may go: the ROM's own use starts about 88 bytes below. So `start.S`
gives the program a stack of its own. It saves the ROM's SP, points SP at `__stack_top` (0x21d800, set in
`hello.ld`), calls `radio_main`, and puts the ROM's SP back before it returns to the ROM, whose handler pops its own
saved registers from its stack. ROM routines our code calls simply run on our stack. With interrupts masked, the
ROM serves nothing until our program returns, so keep programs short.

**2. Places for code and data: the memory map.** Code goes at LOAD (0x210000), loaded after HCI Reset, which clears
it (section 2); the lab's earlier programs keep code and its data in the 4 KB below 0x211000. Data goes in
0x217400–0x21cfff, which the ROM left unchanged while it served HCI and Bluetooth LE traffic, and the stack in the
2 KB below 0x21d800 (step 1). Results go in RESULT_AREA (0x21ff00). Read only addresses on the map: a Read_RAM where
nothing answers hangs the ROM until it is reset [Pi: the lab's ROM probes].

Neither Broadcom nor Cypress publishes the map; CYW gives only the sizes, 576 KB of ROM and 160 KB of RAM (CYW
p. 38). The lab's earlier programs found the rest by reading the ROM's code and probing with Read_RAM:

```text
radio address                                                    used by
0x00000000 +-----------------------------------------------+
           | ROM, 576 KB                                   |    the ROM
           |   0x000000  vector table: SP 0x200400, ...    |
           |   0x00361e  the event routine (step 6)        |
           |   0x0768cc  Launch_RAM's call to our code     |
0x00090000 +-----------------------------------------------+
           | nothing: a read here hangs the ROM            |
0x000d0000 +-----------------------------------------------+
           | patch RAM: Broadcom's fixes to the ROM        |    patches (none loaded)
           :                                               :
0x00200000 +-----------------------------------------------+
           | RAM, at least 128 KB: in detail below         |    the ROM, and us
0x00220000 +-----------------------------------------------+
           :                                               :
0x00300000 | DMA engine, an ARM PL081                      |    the ROM: channels 1, 2
0x00310000 | patch registers                               |    the ROM
0x00318354 | AES-128 engine                                |    free
0x00360000 | the radio's UART, wired to the Pi's PL011     |    the ROM: HCI
0x006c0000 | a window onto the Wi-Fi CPU's RAM             |    the Wi-Fi CPU
           :                                               :
0xe0001000 | DWT: a cycle counter at 0xe0001004            |    free
0xe000e010 | SysTick timer                                 |    free
0xe000e100 | NVIC, the interrupt controller                |    the ROM
0xe000ed00 | SCB: CPUID; VTOR = 0, the ROM's vectors       |    the ROM
           +-----------------------------------------------+
```

The RAM, where our program lives:

```text
0x00200000 +-----------------------------------------------+
           | the ROM's main stack, below 0x200400          |    the ROM
           | the ROM's data, seen from 0x200588 to         |    the ROM
           |   0x209ecb: buffer pools, queues, state       |
0x00210000 +-----------------------------------------------+
           | LOAD: our program (lab programs use 4 KB)     |    us
0x00211000 +-----------------------------------------------+
           | the ROM's HCI buffers, at 0x211b98 and        |    the ROM
           |   0x21279c                                    |
0x00217400 +-----------------------------------------------+
           | unchanged while the ROM served HCI and        |    free: our data
           |   Bluetooth LE traffic                        |
0x0021d000 +-----------------------------------------------+
           | our stack, 2 KB, below __stack_top (0x21d800) |    us
0x0021d800 +-----------------------------------------------+
           | unused: a gap below the ROM's stack           |
0x0021db00 +-----------------------------------------------+
           | the ROM's process stack; SP is near 0x21db58  |    the ROM
           |   when Launch_RAM calls us; it grows down     |
           :   (where it ends is not known)                :
0x0021ff00 | RESULT_AREA, 256 bytes                        |    us
0x00220000 +-----------------------------------------------+
             the lab's programs use RAM up to here; where
             RAM ends is not known
```

The Details at the end list the devices and the M3's registers.

**3. An entry point: `start.S`.** Launch_RAM calls LOAD | 1, so the first instruction at LOAD is our entry, `_start`.
It saves the return address, switches to our stack (step 1), calls `radio_main`, switches back, and returns to the
ROM.

<!-- source:start code-bluetooth/start.S -->

```text
/* The entry point Launch_RAM calls, at LOAD | 1 (0x210001). It never runs on the Pi.
 *
 * Launch_RAM calls it inside the ROM's handler, on the ROM's stack, whose depth
 * nobody knows. So it switches to a stack of its own, __stack_top (hello.ld), calls
 * radio_main, switches back, and returns to the ROM, which goes on serving HCI.
 * Interrupts are masked meanwhile, so only our code uses our stack. It clears no
 * BSS, so radio code must not count on zeroed statics. */
    .syntax unified
    .cpu cortex-m3
    .thumb
    .section .text.start, "ax", %progbits
    .global _start
    .thumb_func
_start:
    push  {r4, lr}          @ on the ROM's stack; r4 keeps it 8-byte aligned
    mov   r4, sp            @ keep the ROM's SP: C preserves r4
    ldr   r0, =__stack_top  @ our own stack
    mov   sp, r0
    bl    radio_main
    mov   sp, r4            @ back to the ROM's stack
    pop   {r4, pc}          @ back to the ROM
    .ltorg
```

<!-- source:end -->

**4. Linked where it is loaded: `hello.ld`.** `chip.S` never refers to its own address, so it runs wherever it is
loaded. C code does refer to its own addresses: `hello.c` reads its text from 0x210048. So the program is linked at
LOAD, with `_start`'s section first.

<!-- source:start code-bluetooth/hello.ld -->

```text
/* Link the radio's program at LOAD (0x210000, in bt.h; keep the two equal), where
 * bt_write_ram puts it. _start comes first, so Launch_RAM at LOAD | 1 calls it
 * (bit 0 = Thumb). The program's stack is its own: 2 KB below __stack_top, in RAM
 * the ROM leaves alone (the memory map, in the hello-world card). */
SECTIONS {
    . = 0x210000;
    .text : { *(.text.start) *(.text*) *(.rodata*) }
    .data : { *(.data*) }
    .bss  : { *(.bss*) *(COMMON) }
}
__stack_top = 0x21d800;
```

<!-- source:end -->

**5. A build for the M3: the Makefile.** As with `chip.S`, `make` turns the program into a C array, `hello_image.h` (in `objs/`),
which the Pi program, `4-bt-hello.c` (below), includes:

<!-- source:start code-bluetooth/Makefile:22-29 -->

```make
# The radio's hello world: C for the Cortex-M3, linked at LOAD (hello.ld), as bytes.
$(BUILD_DIR)/hello_image.h: start.S hello.c hello.ld
	@mkdir -p $(BUILD_DIR)
	arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Os -ffreestanding -nostdlib -Wall -Werror \
	    -T hello.ld start.S hello.c -o $(BUILD_DIR)/hello.elf
	arm-none-eabi-objcopy -O binary $(BUILD_DIR)/hello.elf $(BUILD_DIR)/hello.raw
	xxd -i < $(BUILD_DIR)/hello.raw > $@
$(BUILD_DIR)/4-bt-hello.o: $(BUILD_DIR)/hello_image.h
```

<!-- source:end -->

`-mcpu=cortex-m3 -mthumb` compiles for the radio's CPU, which runs only Thumb code. `-ffreestanding -nostdlib`
leaves out the C library and its start-up code, so `start.S` is the only entry code. `-T hello.ld` places the
program at LOAD (0x210000) and sets `__stack_top`. `objcopy` keeps only the program's bytes, and `xxd -i` writes them as a C array.

**6. A way to report back.** There are two:

1. **The result area, RESULT_AREA (0x21ff00).** It is only a convention: any free RAM on the map works. The program
   stores its results there with ordinary stores, and after the program returns the Pi reads them with Read_RAM, as
   `bt_run` does in section 4. The Pi sees only what the program left at the end.
2. **HCI events, through the ROM's event routine.** The ROM sends its own events through one routine, at 0x361e
   [disassembly; lab card 04]. `hello.c` calls it as `ROM_SEND_EVENT` (0x361e | 1, bit 0 for Thumb) with an event {0xff, length,
   text}, and the ROM sends it to the Pi like any other event. Events can carry text, and a program can queue many:
   there is room for 32 [Pi: lab card 27]. The routine's address is right only for the tested ROM, so the Pi checks the ROM's
   version before the launch.

The hello world uses the second:

<!-- source:start code-bluetooth/hello.c -->

```c
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
```

<!-- source:end -->

**7. No C runtime.** Nothing clears the BSS, so set statics before use; `hello.c`'s one static is `const`, so it is
part of the program's bytes. There is no C library: if gcc emits a call to `memcpy` or `memset` on its own, the link
fails, and you supply your own. And one Write_RAM carries at most 251 data bytes (`bt_write_ram` asserts this): the
hello world is 101 bytes, and a bigger program must be loaded in pieces.

**The Pi side,** `4-bt-hello.c`. It brings the radio up, refuses any other ROM, loads and launches the program,
prints the text that comes back, and checks that the ROM still answers and that the program ran on its own stack:

<!-- source:start code-bluetooth/4-bt-hello.c -->

```c
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
```

<!-- source:end -->

**Why each step.**

1. **Bring the radio up as the driver does** (sections 1–3), so the ROM has just restarted.
2. **Refuse any ROM but the tested one (`ROM_VERSION`),** because `ROM_SEND_EVENT` (0x361e | 1) is right only for
   this ROM; in another, calling it could hang the radio. Read_Local_Version_Information (`READ_LOCAL_VERSION`,
   0x1001) returns the version (BT Part E 7.4.1). Its LMP_Subversion, 0x2209 (`ROM_VERSION`'s last two bytes), is
   the one Linux's btbcm driver names "BCM43430A1" (`btbcm.c`, line 506).
3. **Read events until the vendor event arrives.** `bt_launch_ram` has already read Launch_RAM's Command Complete,
   which the ROM queues before it calls our code (section 4, note 2). Our event follows; the loop skips anything else.
4. **Print the vendor event's text.** HCI reserves event code 0xff "for vendor-specific debugging events" (BT Part E
   5.4.4), so no standard event can be mistaken for ours. The Pi ends the text itself, NUL or not.
5. **Ask for the version again.** An answer shows that our program returned and the ROM still serves commands.
6. **Look for the text below `RADIO_STACK_TOP` (0x21d800).** `radio_main` built its event on its stack, so finding
   the text in the RAM just below shows that the program ran on its own stack.

**How we know it works.** The Pi printed "the Bluetooth CPU says: hello from the Bluetooth CPU [29 bytes]", the ROM
then answered again, and the text was in the RAM just below 0x21d800 [Pi: session 10]. That text exists only in the
bytes we loaded, so our code built the event, on its own stack, and returned to the ROM cleanly.

**Notes.**

1. **The ROM's version,** `ROM_VERSION`, read 07 00 00 07 0f 00 09 22 [Pi]: HCI_Version 7, HCI_Subversion 0, LMP_Version 7,
   Company_Identifier 0x000f, which Bluetooth's Assigned Numbers give to Broadcom, and LMP_Subversion 0x2209 (BT
   Part E 7.4.1).
2. **Our check is only the version.** Before calling other ROM routines, also check that no ROM patch is enabled:
   the patch enable bits, 16 bytes at 0x310204, must all read 0 [Pi: lab card 04].

## Details

1. `-Os` keeps the program small. `hello.ld` puts `.text.start`, `_start`'s section, first.
2. The 101 bytes: `_start` at 0x210000, with `__stack_top` as a literal at 0x210010; `radio_main` at 0x210014; a
   literal holding the text's address at 0x210044; and the 29 bytes of text at 0x210048. `ROM_SEND_EVENT`,
   0x361e | 1 = 0x361f, is built into a `movw` instruction. `radio_main` uses 40 bytes of our stack, and `_start` 8
   bytes of the ROM's [derived: `arm-none-eabi-objdump -d objs/l1/l2/l3/hello.elf`].
3. The call goes to 0x361f, not 0x361e: bit 0 selects Thumb, as for Launch_RAM at LOAD | 1 (0x210001) (ARMv7-M A2.3.1,
   p. A2-31).
4. On the wire the event is 04 ff 1d, then the 29 bytes: 04 is an HCI event (BT Part A, Table 2.1), and 0x1d = 29 is
   its length.
5. The ROM uses the routine for its own status-only Command Complete events: its event builder at 0x3648 calls it at
   0x3666, and Launch_RAM's handler at 0x768c4 [disassembly; lab card 04, README].
6. The map's sources, by part:
   - ROM: lab card 04's `EVIDENCE.md` (0x0–0x8ffff read whole, the same on two boards).
   - The failed reads: the lab's `devices/radio/BRINGUP-BT.md`, line 289.
   - RAM: lab card 31 (128 KB) and the lab's network debugger (`network-debugger/radio/memmap`: 0x217400–0x21daff
     unchanged under traffic).
   - The ROM's HCI buffers: lab card 30.
   - The devices: the lab cards named in the devices table below, and the lab's register list, `labs/radio-processors/code/support/bt/lib/registers.h`.
   - What the map leaves out: timers at 0x32a000, a PL061 GPIO block at 0x32b000, a random number generator at
     0x352600 and the Bluetooth/Wi-Fi coexistence block at 0x650000, which the lab has only read.
7. CYW's 160 KB of RAM is more than the 128 KB at 0x200000. The rest may be the patch RAM at 0xd0000; that is not
   established. Where the RAM ends, and where the ROM's data and stack end, is not known.
8. Every address up to 0x6c0000 lies in what ARMv7-M calls the Code region (Table B3-1, p. B3-592). With no MPU,
   the M3 treats the radio's device registers there as Normal memory, not Device memory.
9. Launch_RAM's context: Thread mode, on the ROM's process stack (CONTROL = 2), with interrupts masked (PRIMASK = 1;
   ARMv7-M B1.4.3, p. B1-519), at entry and after the call to the ROM's event routine, on every launch on two boards [Pi: lab card 04].

**The devices.** The lab's earlier programs found them by reading the ROM's code. Of these, only the DMA engine has a
public manual: ARM's PL081 TRM, `docs/pl081-dma.pdf`, with its errata in `docs/pl081-errata.pdf`;
`docs/pl080-dma.pdf` covers the eight-channel PL080.

| Address | Device | What the lab's programs found | Lab card |
|---|---|---|---|
| 0x300000 | DMA engine, an ARM PL081 | Off at power-on. The ROM turns it on for Write_RAMs of 60 data bytes or more, and for Read_RAM answers of 58 or more, using channels 2 and 1. The cards use channel 0. Its ID says PL081, a two-channel part (PL081 TRM, p. 1-2), yet channel 2 works: measured, not explained | 02, 26, 30 |
| 0x310000 | patch registers: 128 addresses, enable bits at 0x310204 | All off: we load no patches | 04 |
| 0x318354 | AES-128 engine: input 0x318354, key 0x318bf8, control 0x318c08, output 0x318c2c, status 0x318c40 | It encrypts the AES standard's test block | 05 |
| 0x360000 | the radio's UART: data at 0x36001c | The ROM's HCI link to the Pi. The mode word at 0x360420 drives RTS, and bit 0 of 0x360428 is CTS | 09, 19 |
| 0x6c0000 | a window onto the Wi-Fi CPU's RAM at 0x40000 | 32 bytes tested; calling code there hung HCI | 03 |

**The M3's own registers,** with the values the lab's programs read:

| Address | Register | Value | ARMv7-M |
|---|---|---|---|
| 0xe0001004 | DWT_CYCCNT, the cycle counter | counts at about 24 MHz, the M3's clock [Pi: lab card 27] | Table C1-1, p. C1-683 |
| 0xe000e010 | SysTick, a 24-bit timer | lab card 08 uses it as a 10 ms timer | Table B3-7, p. B3-621 |
| 0xe000e100 | the NVIC's interrupt enables | the radio's UART is interrupt 15 [lab card 04]; the DMA engine is 16, in the lab's DMA code | Table B3-8, p. B3-626 |
| 0xe000ed00 | CPUID | 0x412fc231: a Cortex-M3, revision r2p1 [Pi: lab cards 20, 27 and 29] | B3.2.3, p. B3-598 |
| 0xe000ed08 | VTOR, the vector table's address | 0: the ROM's vectors [Pi: lab cards 04 and 08] | Table B3-4, p. B3-596 |
| 0xe000ed90 | MPU_TYPE | 0: there is no MPU [Pi: lab card 20] | Table B3-11, p. B3-635 |

## Check it yourself: what to read

**The Bluetooth spec, Linux and the M3.** The crash course's end note says how to find a page.

1. BT Vol 4 Part E 7.4 and 7.4.1: Read_Local_Version_Information and its return fields.
2. BT Vol 4 Part E 5.4.4: the event packet, and event code 0xFF, "reserved for vendor-specific debugging events".
3. `docs/fetched/btbcm.c`, line 506: 0x2209 is "BCM43430A1". `docs/fetched/assigned-numbers/company_identifiers.yaml`,
   line 12135: 0x000F is Broadcom.
4. ARMv7-M A2.3.1, p. A2-31: the Thumb bit, here of the ROM routine's address.
5. `docs/pl081-dma.pdf`, section 1.1, p. 1-2: the PL081 has two DMA channels.
6. For the map: CYW section 9.1, p. 38 (the sizes); ARMv7-M Table B3-1, p. B3-592 (the address map), Table B3-4,
   p. B3-596 (the SCB), B3.2.3, p. B3-598 (CPUID), Table B3-7, p. B3-621 (SysTick), Table B3-8, p. B3-626 (the NVIC),
   Table B3-11, p. B3-635 (the MPU), Table C1-1, p. C1-683 (the DWT), and B1.4.3, p. B1-519 (PRIMASK and CONTROL).
