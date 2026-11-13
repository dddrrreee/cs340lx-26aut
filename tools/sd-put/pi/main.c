// sd-put, pi side. The laptop (../sd-put) sends a little job file:
//
//     cp go 23389 34816        <- command, go|dry[+boot], FAT date, FAT time
//     f0 LIBPI.SO              <- for cp: what to GET from the laptop, name on the card
//     f1 0-STATIC.ELF          <- for rm and get: "- NAME"
//
// "+boot" also allows writing boot files (KERNEL.IMG, ...).
//
// Everything is checked before the first write, and in "dry" mode we stop
// right after the checks.
#include "rpi.h"
#include "sdfs.h"
#include "xfer.h"

#define MAX_FILES 32
#define FILE_BUF ((uint8_t *)0x0A000000) // well clear of us, the stack and the heap

static char job[4096];
static char *key[MAX_FILES], *name[MAX_FILES];
static uint8_t *data[MAX_FILES];
static uint32_t size[MAX_FILES];
static int nfiles;

static sd_file others[128];    // files we don't touch, and their crcs before
static uint32_t others_crc[128];
static int nothers;

// next word of the job file
static char *word(char **p) {
    while (**p == ' ' || **p == '\n')
        (*p)++;
    char *w = *p;
    while (**p && **p != ' ' && **p != '\n')
        (*p)++;
    if (**p)
        *(*p)++ = 0;
    return w;
}

static unsigned number(char **p) {
    unsigned v = 0;
    for (char *w = word(p); *w; w++)
        v = v * 10 + (*w - '0');
    return v;
}

static int ours(const char *n) {
    for (int i = 0; i < nfiles; i++)
        if (strcmp(name[i], n) == 0)
            return 1;
    return 0;
}

static void ls(void) {
    static sd_file f[128];
    int n = sd_list(f, 128);
    for (int i = 0; i < n; i++) {
        printk("  %s", f[i].name);
        for (int pad = strlen(f[i].name); pad < 14; pad++)
            printk(" ");
        printk("%d\n", f[i].size);
    }
    printk("  %d files, %d MB free\n", n, sd_free_kb() / 1024);
}

void notmain(void) {
    job[xfer_get("@job", job)] = 0;
    char *p = job;
    char *cmd = word(&p), *mode = word(&p);
    sd_date = number(&p);
    sd_time = number(&p);
    while (nfiles < MAX_FILES && *(key[nfiles] = word(&p)))
        name[nfiles++] = word(&p);

    sd_mount();
    if (strcmp(cmd, "get") == 0) {
        for (int i = 0; i < nfiles; i++) {
            xfer_send_start(sd_size(name[i]), sd_crc(name[i]));
            sd_read(name[i], xfer_send);
        }
        printk("SD-PUT: OK\n");
        return;
    }
    int len = strlen(mode);
    sd_allow_boot_files = len > 5 && strcmp(mode + len - 5, "+boot") == 0;
    if (strcmp(cmd, "ls") == 0) {
        ls();
        printk("SD-PUT: OK\n");
        return;
    }
    int cp = strcmp(cmd, "cp") == 0;

    // 1. check everything first: any problem stops us before the first write
    uint32_t need = 0; // KB
    int new_names = 0;
    for (int i = 0; i < nfiles; i++) {
        sd_check_name(name[i]);
        if (sd_allow_boot_files)
            printk("  (boot files allowed for this run)\n");
        if (!cp) {
            if (!sd_exists(name[i]))
                panic("there's no <%s> on the card\n", name[i]);
            printk("  rm %s\n", name[i]);
            continue;
        }
        data[i] = i ? data[i - 1] + ((size[i - 1] + 3) & ~3) : FILE_BUF;
        size[i] = xfer_get(key[i], data[i]);
        need += size[i] / 1024 + 64; // KB, rounded up generously
        new_names += !sd_exists(name[i]);
        printk("  cp %s (%d bytes)%s\n", name[i], size[i], sd_exists(name[i]) ? ", replacing the old one" : "");
    }
    if (cp && need > sd_free_kb())
        panic("not enough free space on the card\n");
    if (cp && new_names > sd_free_slots())
        panic("root directory is full\n");
    if (strncmp(mode, "dry", 3) == 0) {
        printk("dry run: all checks passed, nothing written\n");
        printk("SD-PUT: OK\n");
        return;
    }

    // 2. checksum every other file, so afterwards we can show we didn't touch them
    static sd_file all[128];
    int n = sd_list(all, 128);
    for (int i = 0; i < n; i++) {
        if (ours(all[i].name))
            continue;
        others[nothers] = all[i];
        others_crc[nothers++] = sd_crc(all[i].name);
    }

    // 3. do it, then read back what we wrote
    for (int i = 0; i < nfiles; i++) {
        if (!cp) {
            sd_rm(name[i]);
        } else {
            sd_put(name[i], data[i], size[i]);
            if (sd_crc(name[i]) != crc32_add(0, data[i], size[i]))
                panic("reading back %s gave different bytes!\n", name[i]);
        }
    }

    // 4. everything else is exactly as it was
    for (int i = 0; i < nothers; i++)
        if (sd_crc(others[i].name) != others_crc[i])
            panic("%s CHANGED. This should never happen.\n", others[i].name);
    printk("done; %d other files checked unchanged\n", nothers);
    printk("SD-PUT: OK\n");
}
