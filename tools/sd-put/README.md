# sd-put

Copies files to and from the pi's SD card over the UART, so you don't need a
card reader. (I forgot mine in the city.)

## Usage

    sd-put ls                      # list files on the card
    sd-put cp libpi.so 0-dyn.elf   # copy files to the card
    sd-put cp a.out:HELLO.ELF      # copy under a different name
    sd-put rm HELLO.ELF            # remove a file
    sd-put get KERNEL.IMG          # copy a file from the card to here

- `-y`: don't ask for confirmation
- `-n`: dry run (do all the checks, write nothing)
- `--boot`: let `cp` replace a boot file such as `kernel.img`. If the new
  one doesn't boot, you'll need a card reader to fix it.

Install with `make -C tools/sd-put install` (puts `sd-put` in `~/bin`).

Files go in the root directory, with 8.3 names: `libpi.so` is stored as
`LIBPI.SO`. sd-put won't run if the root directory has subdirectories,
and it can't replace or remove files that have long names.

## How it works

`sd-put` sends `pi/sd-put.bin` through the normal bootloader, the same way
`pi-install` does. The pi program then asks the laptop for a job file and for
each file it needs, and does the work on the card.

- `sd-put` (laptop): boots the pi program, answers its file requests, saves
  any files it sends back.
- `pi/xfer.c`: moves files over the UART.
  - pi → laptop: `0x01 "GET <name>\n"`
  - laptop → pi: `"FILE" <u32 size> <u32 crc32> <bytes>`
  - pi → laptop, for `get`: `0x02 <u32 size> <u32 crc32> <bytes>`
- `pi/main.c`: reads the job file and runs `ls`/`cp`/`rm`/`get`.
- `pi/sdfs.c`: adds and removes files in the FAT32 root directory. It uses
  lab 6's FAT32 code to read the card and adds the writes. The safety rules:
  - it never writes before the first FAT (MBR, boot sector)
  - new data only goes into clusters the FAT marks as free
  - before changing a FAT or directory entry, it re-reads that sector and
    stops if the sector doesn't hold what it expected
  - `cp` writes the data and its FAT chain, reads the data back, and only
    then writes the directory entry. Replacing a file is a single
    directory-entry write, so the card always has a complete copy.
  - all checks (names, free space, FAT consistency) run before the first
    write. Afterwards it verifies that every other file's checksum is
    unchanged.

Tested on a Pi Zero with a 32GB card.
