#include "rpi-asm.h"

.section ".text.boot"
.globl _start
_start:
    mov r0, #SUPER_MODE
    orr r0, r0, #(1<<7)     @ interrupts off
    msr cpsr, r0
    prefetch_flush(r1)
    mov sp, #STACK_ADDR

    @ zero .bss ourselves: lab 6's libpi _cstart doesn't (its ELF loader
    @ does), and RAM still holds whatever the last program left there
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
1:  cmp r0, r1
    strlo r2, [r0], #4
    blo 1b

    bl _cstart              @ libpi: cycle counter, then notmain()
    bl clean_reboot         @ prints DONE!!! so the laptop side knows we're done
