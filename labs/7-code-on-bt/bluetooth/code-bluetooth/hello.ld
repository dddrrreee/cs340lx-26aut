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
