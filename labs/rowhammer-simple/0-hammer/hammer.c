#include "rpi.h"
#include "vm-enable.h"

#define ARENA_BASE    0x4000000u
#define ARENA_MB      16
#define REGION_WORDS  ((1024 * 1024) / 4)
#define ITERS         2000000u
#define ROW_STRIDE    (32 * 1024)      // same-bank adjacent-row distance (see Geometry).

static volatile uint32_t *const arena = (volatile uint32_t *)ARENA_BASE;

// alternating stripes: even rows all-zero (victims), odd rows all-ones (aggressors).
static inline uint32_t stripe_of(uint32_t off) {
    return ((off / ROW_STRIDE) & 1) ? 0xffffffffu : 0;
}

static inline void hammer(volatile uint32_t *a, volatile uint32_t *b) {
    unimplemented();                              // two stores of all-ones
}

static unsigned long run_row(void) {
    for (unsigned i = 0; i < REGION_WORDS; i++)   // set alternating stripes
        arena[i] = stripe_of(i * 4);
    dsb();

    // hammer the all-ones rows on each side of every all-zero row
    unimplemented();

    dsb();
    unsigned long flips = 0;                       // count 0->1 flips in the zero rows
    for (unsigned i = 0; i < REGION_WORDS; i++)
        if (arena[i] != stripe_of(i * 4)) flips++;
    return flips;
}

void notmain(void) {
    vm_on_cache();                                // map arena, caches on
    ksec_t k = ksec_default(ARENA_BASE, dom_kern);
    k.n_mb = ARENA_MB;
    k.attr = MEM_wb_alloc;
    ksec_map(5, &k);
    dsb();

    output("TOTAL flips: %d\n", (unsigned)run_row());
}
