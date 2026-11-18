#include "rpi.h"
#include "vm-enable.h"
#include "dma-impl.h"

#define ARENA_BASE    0x4000000u
#define ARENA_MB      16
#define REGION_BYTES  (1024 * 1024)
#define REGION_WORDS  (REGION_BYTES / 4)
#define ITERS         2000000u
#define ROW_STRIDE    (32 * 1024)
#define DMA_CH        5
#define GOLDEN        ((uint32_t *)(ARENA_BASE + 8u * 1024 * 1024))

static volatile uint32_t *const arena = (volatile uint32_t *)ARENA_BASE;

static inline uint32_t stripe_of(uint32_t off) {
    return ((off / ROW_STRIDE) & 1) ? 0xffffffffu : 0;
}

// reuse 0-hammer
static unsigned long run_row(void) {
    unimplemented();
}

// turn an ARM pointer into the DMA's UNCACHED view of that same DRAM
static inline bus_t bus_uncached(volatile void *p) {
    unimplemented();
}

// self-looping DMA that copies golden to arena
static dma_ch_t *start_refresh(cb_t *cb) {
    unimplemented();
}

// cs bit set to 31
static void stop_refresh(dma_ch_t *d) {
    unimplemented();
}

void notmain(void) {
    vm_on_cache();
    ksec_t k = ksec_default(ARENA_BASE, dom_kern);
    k.n_mb = ARENA_MB;
    k.attr = MEM_wb_alloc;
    ksec_map(5, &k);
    dsb();

    for (unsigned i = 0; i < REGION_WORDS; i++)   // a clean copy for the DMA to replay
        GOLDEN[i] = stripe_of(i * 4);
    dsb();

    cb_t *cb = kmalloc_aligned(sizeof *cb, 32);   // control block: 32-byte aligned, in DRAM
    dma_ch_t *d = start_refresh(cb);              // refresh ring runs concurrently...
    unsigned long flips = run_row();              // ...with YOUR 0-hammer attack
    stop_refresh(d);

    output("DMA-refresh defense -> TOTAL flips: %d\n", (unsigned)flips);
}
