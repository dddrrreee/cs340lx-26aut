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
