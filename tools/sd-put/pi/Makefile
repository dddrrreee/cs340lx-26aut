# The pi side of sd-put. ../sd-put builds this for you.
# The implementation is checked in as object files; `make` just links them
# against lab 6's libpi (its FAT32 read code and SD card driver).
LIBPI ?= ../../../labs/6-elf-dynamic-linker/0-my-libpi
OBJS = start.o main.o sdfs.o xfer.o

CC = arm-none-eabi-gcc

sd-put.bin: $(OBJS) memmap $(LIBPI)/libpi.a
	arm-none-eabi-ld -T memmap $(OBJS) $(LIBPI)/libpi.a $(shell $(CC) -print-libgcc-file-name) -o sd-put.elf
	arm-none-eabi-objcopy sd-put.elf -O binary $@

$(LIBPI)/libpi.a:
	$(MAKE) -C $(LIBPI)

clean:
	rm -f sd-put.elf sd-put.bin
.PHONY: clean
