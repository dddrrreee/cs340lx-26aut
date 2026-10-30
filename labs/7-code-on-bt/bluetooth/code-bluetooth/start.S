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
