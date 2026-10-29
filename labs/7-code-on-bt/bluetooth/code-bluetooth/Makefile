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
# the radio programs' bytes, chip_image.h and hello_image.h, are built in BUILD_DIR (objs/)
CFLAGS += -I$(BUILD_DIR)

# chip.S, the Bluetooth CPU's program: assemble it for the Cortex-M3 and take its bytes.
$(BUILD_DIR)/chip_image.h: chip.S
	@mkdir -p $(BUILD_DIR)
	arm-none-eabi-as -mcpu=cortex-m3 -mthumb $< -o $(BUILD_DIR)/chip.o
	arm-none-eabi-objcopy -O binary $(BUILD_DIR)/chip.o $(BUILD_DIR)/chip.bin
	xxd -i < $(BUILD_DIR)/chip.bin > $@
$(BUILD_DIR)/1-bt-bringup.o $(BUILD_DIR)/3-test-bt-run.o: $(BUILD_DIR)/chip_image.h

# The radio's hello world: C for the Cortex-M3, linked at LOAD (hello.ld), as bytes.
$(BUILD_DIR)/hello_image.h: start.S hello.c hello.ld
	@mkdir -p $(BUILD_DIR)
	arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Os -ffreestanding -nostdlib -Wall -Werror \
	    -T hello.ld start.S hello.c -o $(BUILD_DIR)/hello.elf
	arm-none-eabi-objcopy -O binary $(BUILD_DIR)/hello.elf $(BUILD_DIR)/hello.raw
	xxd -i < $(BUILD_DIR)/hello.raw > $@
$(BUILD_DIR)/4-bt-hello.o: $(BUILD_DIR)/hello_image.h
