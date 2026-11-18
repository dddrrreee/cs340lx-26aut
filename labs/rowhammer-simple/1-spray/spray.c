#include "rpi.h"
#include "vm-enable.h"
#include "pte-vm.h"

#define ARENA_BASE    0x4000000u
#define ARENA_MB      16
#define ARENA_WORDS   (ARENA_MB * 1024 * 1024 / 4)
#define ITERS         2000000u
#define ROW_STRIDE    (32 * 1024)

static volatile uint32_t *const arena = (volatile uint32_t *)ARENA_BASE;

// even rows hold our sprayed descriptor (-> F_att); odd rows are all-ones aggressors.
static inline uint32_t stripe_of(uint32_t off) {
    return ((off / ROW_STRIDE) & 1) ? 0xffffffffu : SANDBOX_DESC;
}

// from 0-hammer
static inline void hammer(volatile uint32_t *a, volatile uint32_t *b) {
    unimplemented();
}

// which physical section does a descriptor point at?
static inline uint32_t desc_base_sec(uint32_t w) {
    unimplemented();
}

// check if section <sec> one of the enclave's secret frames?
static int is_secret(uint32_t sec) {
    for (unsigned i = 0; i < N_SECRET; i++)
        if (secret_sec[i] == sec) return 1;
    return 0;
}

// has this sprayed descriptor been redirected onto a secret frame? (see README)
static int redirects_to_secret(uint32_t w) {
    unimplemented();
}

// map a virtual address you own onto the physical section sec, then deref it
static const char *probe_read(uint32_t sec) {
    unimplemented();
}

// spray the descriptors, hammer your own rows, and return the arena index of a
// descriptor that got redirected onto a secret (0 = none this sweep)
static unsigned spray_and_hunt(uint32_t *win) {
    unimplemented();
    return 0;
}

void notmain(void) {
    vm_on_cache();
    pte_lab_setup(ARENA_BASE, ARENA_MB, "in-n-out-is-awesome");   // given

    uint32_t win;
    unsigned w = spray_and_hunt(&win);
    if (!w) { output("no descriptor redirected this sweep\n"); return; }

    output("PWNED: arena word %x flipped %x -> %x (frame %x -> %x)\n",
           w * 4, SANDBOX_DESC, win, desc_base_sec(SANDBOX_DESC), desc_base_sec(win));
    output("deref our own VA through the flipped frame -> \"%s\"\n",
           probe_read(desc_base_sec(win)));
}
