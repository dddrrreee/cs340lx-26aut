#ifndef __SDFS_H__
#define __SDFS_H__
// Just enough FAT32 to add and remove files in the root directory of the
// pi's SD card. Names are upper-case 8.3 ("LIBPI.SO").
#include <stdint.h>

typedef struct {
    char name[13];
    uint32_t size;
} sd_file;

// Read the card: partition table, FAT, root directory. Stops if anything
// looks off (see sdfs.c).
void sd_mount(void);

// Files in the root directory. Returns how many (at most <max>).
int sd_list(sd_file *files, int max);

// Stops with an error if <name> isn't a name we're willing to write.
void sd_check_name(const char *name);

int sd_exists(const char *name);
uint32_t sd_free_kb(void);
int sd_free_slots(void);           // how many new files the root dir can take
uint32_t sd_crc(const char *name); // crc32 of a file's contents, read off the card

uint32_t sd_size(const char *name);
// Read a file off the card, handing it to <f> one piece at a time.
void sd_read(const char *name, void (*f)(const void *, uint32_t));

// Let sd_put write boot files (only `sd-put cp --boot`). Off by default.
extern int sd_allow_boot_files;

// Write a file / remove one. Replacing is atomic: the new copy is written
// and read back before the directory entry is switched over to it.
void sd_put(const char *name, const void *data, uint32_t n);
void sd_rm(const char *name);

// Timestamp for new files (FAT format; the laptop sends us its clock).
extern uint16_t sd_date, sd_time;
#endif
