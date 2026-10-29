# x-stray-word.c (section 1, note 4) has no .out: make PROGS=x-stray-word.c
PROGS = 0-test-pl011.c 1-bt-bringup.c 2-test-bt-reset.c 3-test-bt-run.c 4-bt-hello.c
COMMON_SRC = bt.c
RUN ?= 0
CAN_EMIT ?= 0
BOOTLOADER = my-install
RUN=1
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

# The radio's hello world: C for the Cortex-M3, linked at LOAD (hello.ld), as bytes.
hello_image.h: start.S hello.c hello.ld
	arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Os -ffreestanding -nostdlib -Wall -Werror \
	    -T hello.ld start.S hello.c -o hello.elf
	arm-none-eabi-objcopy -O binary hello.elf hello.raw
	xxd -i < hello.raw > $@
$(BUILD_DIR)/4-bt-hello.o: hello_image.h
clean::
	rm -f hello.elf hello.raw hello_image.h

