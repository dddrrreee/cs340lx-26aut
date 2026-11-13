// sdfs: add/remove files in the SD card's root directory.
//
// Reading the card (MBR, boot sector, FAT) is lab 6's fat32 code; this file
// adds writing. What keeps it from wrecking the card:
//   1. nothing before the first FAT is ever written (MBR, boot sector, FSInfo)
//   2. file data only goes into clusters the FAT says are free
//   3. before changing a FAT entry or directory entry, we re-read that
//      sector and check it still says what we think it says
//   4. order: data, then FAT, then the directory entry. If we die partway,
//      the worst case is some lost free space.
//   5. boot files (kernel.img, start.elf, ...) are off limits
#include "rpi.h"
#include "pi-sd.h"
#include "fat32.h"
#include "fat32-helpers.h"
#include "mbr.h"
#include "sdfs.h"
#include "xfer.h"

#define SEC 512
#define MAX_DIR 2048    // root dir entries we handle (16 per sector)

extern fat32_boot_sec_t boot_sector;  // filled in by fat32_mk

uint16_t sd_date, sd_time;
int sd_allow_boot_files;

static fat32_fs_t fs;
static uint32_t last_lba, max_cluster, cluster_size;
static fat32_dirent_t dir[MAX_DIR];   // the root directory...
static uint32_t dir_lba[MAX_DIR / 16]; // ...and the sector each 16 entries came from
static int ndir;

static uint32_t fat_get(uint32_t c) { return fs.fat[c] & 0x0fffffff; }
static int is_end(uint32_t v) { return v >= 0x0ffffff8; }

static uint32_t cluster_lba(uint32_t c) {
    if (c < 2 || c > max_cluster)
        panic("bad cluster number %d\n", c);
    return fs.cluster_begin_lba + (c - 2) * fs.sectors_per_cluster;
}

static int is_file(fat32_dirent_t *d) {
    return d->filename[0] && d->filename[0] != 0xe5 && !fat32_dirent_is_lfn(d)
        && !(d->attr & (FAT32_VOLUME_LABEL | FAT32_DIR));
}

// Every write to the card goes through here (rule 1).
static void write_sector(uint32_t lba, void *buf) {
    if (lba < fs.fat_begin_lba || lba > last_lba)
        panic("refusing to write sector %d: outside the FAT/data area\n", lba);
    pi_sd_write(buf, lba, 1);
}

// Walk every file's cluster chain. If a cluster a file uses is marked free,
// we can't trust "free", so stop before writing anything.
static void check_fat(void) {
    for (int i = 0; i < ndir && dir[i].filename[0]; i++) {
        fat32_dirent_t *d = &dir[i];
        if (d->filename[0] != 0xe5 && !fat32_dirent_is_lfn(d) && (d->attr & FAT32_DIR))
            panic("the root dir has a subdirectory; sd-put doesn't handle that\n");
        if (!is_file(d))
            continue;
        uint32_t c = fat32_cluster_id(d);
        for (uint32_t steps = 0; c; steps++) {
            uint32_t next = fat_get(c);
            cluster_lba(c); // range check
            if (next == 0 || steps > max_cluster)
                panic("the FAT looks broken (cluster %d): not touching this card\n", c);
            c = is_end(next) ? 0 : next;
        }
    }
}

void sd_mount(void) {
    kmalloc_init(FAT32_HEAP_MB);
    pi_sd_init();

    mbr_partition_ent_t part;
    memcpy(&part, mbr_read()->part_tab1, sizeof part);
    if (!mbr_part_is_fat32(part.part_type))
        panic("partition 1 isn't FAT32\n");
    fs = fat32_mk(&part);   // reads the boot sector + the whole FAT

    last_lba = fs.lba_start + boot_sector.nsec_in_fs - 1;
    if (last_lba >= part.lba_start + part.nsec || boot_sector.bytes_per_sec != SEC)
        panic("weird filesystem geometry: not touching this card\n");
    cluster_size = fs.sectors_per_cluster * SEC;
    max_cluster = 1 + (last_lba + 1 - fs.cluster_begin_lba) / fs.sectors_per_cluster;
    if (max_cluster >= fs.n_entries)
        max_cluster = fs.n_entries - 1;

    // read the root directory, remembering where each sector lives
    for (uint32_t c = fs.root_dir_first_cluster; ; c = fat_get(c)) {
        for (int s = 0; s < fs.sectors_per_cluster; s++, ndir += 16) {
            if (ndir == MAX_DIR)
                panic("root directory too big\n");
            dir_lba[ndir / 16] = cluster_lba(c) + s;
            pi_sd_read(&dir[ndir], dir_lba[ndir / 16], 1);
        }
        if (is_end(fat_get(c)))
            break;
    }
    check_fat();
}

/******** names ********/

// "LIBPI.SO" -> "LIBPI   SO " (how FAT stores it). 0 if not a valid 8.3 name.
static int raw_name(const char *name, uint8_t raw[11]) {
    const char *dot = strchr(name, '.');
    int base = dot ? dot - name : strlen(name);
    int ext = dot ? strlen(dot + 1) : 0;
    if (base < 1 || base > 8 || ext > 3)
        return 0;
    memset(raw, ' ', 11);
    memcpy(raw, name, base);
    if (dot)
        memcpy(raw + 8, dot + 1, ext);
    return 1;
}

static int starts(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

void sd_check_name(const char *name) {
    uint8_t raw[11];
    if (!raw_name(name, raw))
        panic("<%s> isn't an 8.3 name\n", name);
    int n = strlen(name);
    if (starts(name, "KERNEL") || starts(name, "START") || starts(name, "BOOTCODE")
            || starts(name, "FIXUP") || strcmp(name, "CONFIG.TXT") == 0
            || strcmp(name, "CMDLINE.TXT") == 0 || (n > 4 && strcmp(name + n - 4, ".DTB") == 0))
        if (!sd_allow_boot_files)
            panic("<%s> is a boot file: not touching it\n", name);
}

static int find(const char *name) {
    uint8_t raw[11];
    if (!raw_name(name, raw))
        return -1;
    for (int i = 0; i < ndir && dir[i].filename[0]; i++)
        if (is_file(&dir[i]) && memcmp(dir[i].filename, raw, 11) == 0)
            return i;
    return -1;
}

int sd_exists(const char *name) { return find(name) >= 0; }

int sd_list(sd_file *files, int max) {
    int n = 0;
    for (int i = 0; i < ndir && dir[i].filename[0] && n < max; i++) {
        if (!is_file(&dir[i]))
            continue;
        fat32_dirent_name(&dir[i], files[n].name);
        for (char *p = files[n].name; *p; p++) // the helper pads with spaces
            if (*p == ' ')
                *p = 0;
        files[n++].size = dir[i].file_nbytes;
    }
    return n;
}

/******** space ********/

uint32_t sd_free_kb(void) {
    uint32_t n = 0;
    for (uint32_t c = 2; c <= max_cluster; c++)
        n += fat_get(c) == 0;
    return n * (cluster_size / 1024);  // KB, so a big card doesn't overflow
}

// A slot is usable if it's past the end of the directory (0x00), or a
// deleted entry (0xe5) that isn't glued to a long-name entry.
static int slot_ok(int i) {
    return dir[i].filename[0] == 0
        || (dir[i].filename[0] == 0xe5 && i > 0 && !fat32_dirent_is_lfn(&dir[i - 1]));
}

int sd_free_slots(void) {
    int n = 0;
    for (int i = 0; i < ndir; i++)
        n += slot_ok(i);
    return n;
}

/******** the two kinds of metadata write ********/

// Set FAT[c] = v in every copy of the FAT (rule 3).
static void fat_set(uint32_t c, uint32_t v) {
    static uint32_t sec[SEC / 4];
    for (int k = 0; k < boot_sector.nfats; k++) {
        uint32_t lba = fs.fat_begin_lba + k * boot_sector.nsec_per_fat + c / (SEC / 4);
        pi_sd_read(sec, lba, 1);
        uint32_t *e = &sec[c % (SEC / 4)];
        if ((*e & 0x0fffffff) != fat_get(c))
            panic("FAT copy %d disagrees about cluster %d: stopping\n", k, c);
        *e = (*e & 0xf0000000) | v;   // top 4 bits are reserved: keep them
        write_sector(lba, sec);
    }
    fs.fat[c] = (fs.fat[c] & 0xf0000000) | v;
}

// Overwrite directory slot <i> with <e> (rule 3).
static void dir_set(int i, fat32_dirent_t *e) {
    static fat32_dirent_t sec[16];
    uint32_t lba = dir_lba[i / 16];
    pi_sd_read(sec, lba, 1);
    if (memcmp(&sec[i % 16], &dir[i], sizeof *e) != 0)
        panic("directory changed under us: stopping\n");
    memcpy(&sec[i % 16], e, sizeof *e);
    write_sector(lba, sec);
    pi_sd_read(sec, lba, 1);
    if (memcmp(&sec[i % 16], e, sizeof *e) != 0)
        panic("directory sector didn't read back right\n");
    memcpy(&dir[i], e, sizeof *e);
}

/******** files ********/

static uint32_t alloc_cluster(void) {
    static uint32_t next = 3;
    for (; next <= max_cluster; next++)
        if (fat_get(next) == 0)
            return next++;
    panic("SD card is full\n");
}

static uint32_t chain_crc(uint32_t c, uint32_t left);

void sd_put(const char *name, const void *data, uint32_t n) {
    sd_check_name(name);
    int old = find(name);   // replacing? then we reuse its directory slot
    int slot = old;
    if (slot < 0) {
        slot = 0;
        while (slot < ndir && !slot_ok(slot))
            slot++;
        if (slot == ndir)
            panic("root directory is full\n");
    }

    // 1. the data, one cluster at a time into free clusters (rule 2).
    //    Each new cluster is marked "end of file", then the previous one is
    //    pointed at it, so the chain is always valid.
    static uint8_t sec[SEC];
    uint32_t first = 0, prev = 0;
    for (uint32_t off = 0; off < n; off += cluster_size) {
        uint32_t c = alloc_cluster();
        for (uint32_t s = 0; s < fs.sectors_per_cluster; s++) {
            uint32_t at = off + s * SEC;
            memset(sec, 0, SEC);
            if (at < n)
                memcpy(sec, (const uint8_t *)data + at, n - at < SEC ? n - at : SEC);
            write_sector(cluster_lba(c) + s, sec);
        }
        fat_set(c, 0x0fffffff);
        if (prev)
            fat_set(prev, c);
        else
            first = c;
        prev = c;
    }

    // 2. read it back off the card before anything points at it
    if (chain_crc(first, n) != crc32_add(0, data, n))
        panic("reading back %s gave different bytes: old copy left alone\n", name);

    // 3. the directory entry: one sector write. Before it, the old file (if
    //    any) is still there; after it, the new one is. Never neither.
    fat32_dirent_t e;
    memset(&e, 0, sizeof e);
    if (old >= 0)
        memcpy(&e, &dir[old], sizeof e);   // keep its name, attributes, creation date
    raw_name(name, e.filename);
    if (old < 0) {
        e.attr = 0x20; // "archive": a normal file
        e.create_date = sd_date;
        e.ctime = sd_time;
    }
    e.access_date = e.mod_date = sd_date;
    e.mod_time = sd_time;
    uint32_t old_first = old >= 0 ? fat32_cluster_id(&dir[old]) : 0;
    e.hi_start = first >> 16;
    e.lo_start = first & 0xffff;
    e.file_nbytes = n;
    dir_set(slot, &e);

    // 4. replacing: now free the old copy's clusters
    for (uint32_t c = old_first; c; ) {
        uint32_t next = fat_get(c);
        fat_set(c, 0);
        c = is_end(next) ? 0 : next;
    }
}

void sd_rm(const char *name) {
    sd_check_name(name);
    int i = find(name);
    if (i < 0)
        panic("there's no <%s> on the card\n", name);
    if (i > 0 && fat32_dirent_is_lfn(&dir[i - 1]))
        panic("<%s> has a long name; not touching it\n", name);

    // directory entry first (the file is gone), then free its clusters
    fat32_dirent_t e;
    memcpy(&e, &dir[i], sizeof e);
    uint32_t c = fat32_cluster_id(&e);
    e.filename[0] = 0xe5;
    dir_set(i, &e);
    while (c) {
        uint32_t next = fat_get(c);
        fat_set(c, 0);
        c = is_end(next) ? 0 : next;
    }
}

static uint8_t cbuf[64 * SEC];

// Read a file's clusters, calling <f> on each piece (in order).
static void read_chain(uint32_t c, uint32_t left, void (*f)(const void *, uint32_t)) {
    if (cluster_size > sizeof cbuf)
        panic("clusters bigger than 32KB\n");
    for (; left; c = fat_get(c)) {
        pi_sd_read(cbuf, cluster_lba(c), fs.sectors_per_cluster);
        uint32_t k = left < cluster_size ? left : cluster_size;
        f(cbuf, k);
        left -= k;
    }
}

static uint32_t crc_acc;
static void crc_piece(const void *p, uint32_t n) { crc_acc = crc32_add(crc_acc, p, n); }

static uint32_t chain_crc(uint32_t c, uint32_t left) {
    crc_acc = 0;
    read_chain(c, left, crc_piece);
    return crc_acc;
}

static int file(const char *name) {
    int i = find(name);
    if (i < 0)
        panic("there's no <%s> on the card\n", name);
    return i;
}

uint32_t sd_crc(const char *name) {
    int i = file(name);
    return chain_crc(fat32_cluster_id(&dir[i]), dir[i].file_nbytes);
}

uint32_t sd_size(const char *name) { return dir[file(name)].file_nbytes; }

void sd_read(const char *name, void (*f)(const void *, uint32_t)) {
    int i = file(name);
    read_chain(fat32_cluster_id(&dir[i]), dir[i].file_nbytes, f);
}
