# The pi side of sd-put. ../sd-put builds this for you.
# Uses lab 6's libpi, which has the FAT32 read code and the SD card driver.
LIBPI ?= ../../../labs/6-elf-dynamic-linker/0-my-libpi
OBJS = start.o main.o sdfs.o xfer.o

CC = arm-none-eabi-gcc
CFLAGS = -D__RPI__ -Og -Wall -Werror -nostdlib -nostartfiles -ffreestanding -std=gnu99 \
         -mcpu=arm1176jzf-s -mno-unaligned-access -Wno-pointer-sign \
         -I$(LIBPI) -I$(LIBPI)/include -I$(LIBPI)/libc -I$(LIBPI)/src

sd-put.bin: $(OBJS) memmap $(LIBPI)/libpi.a
	arm-none-eabi-ld -T memmap $(OBJS) $(LIBPI)/libpi.a $(shell $(CC) -print-libgcc-file-name) -o sd-put.elf
	arm-none-eabi-objcopy sd-put.elf -O binary $@

$(LIBPI)/libpi.a:
	$(MAKE) -C $(LIBPI)

%.o: %.c sdfs.h xfer.h
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o sd-put.elf sd-put.bin
