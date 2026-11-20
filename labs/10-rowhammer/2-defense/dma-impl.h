#pragma once

// This file defines does a few different things:
//  1. defines a `bus_t` type, since we can't pass normal ARM pointers into the DMA
//  2. defines a `cb_t` type (Control Block type) and some helpers for manipulating them
//  3. defines helpers for manipulating `bus_t` values
//  4. defines a bump allocator helpful for implementing more complex operations in DMA
//  5. defines APIs for interacting with the DMA peripheral

#include "rpi.h"

/* size_t comes from rpi.h <stddef.h> */

// =================================================================================================
// SECTION: BASE DEFINITIONS
// =================================================================================================

// Since the DMA engine lives on the BCM and not the ARM, it has a slightly different view of memory
// than the one we see; thus, we need some helpers for translating addresses.
//
// In particular, we need:
//  - bus addresses for L2 coherent DRAM (i.e. normal ARM main memory)
//  - bus addresses for peripherals
//
// Note that the "L2" here is the GPU's L2 cache, which is shared with the ARM since the ARM has no
// L2 cache (only L1).
// 
// NOTE: there's a lot of wrong information out there about how 'bus addresses' work, so be careful
//       if you're trying to read up on this on your own.
// 
// NOTE: while it seems like it seems like it should be fine to just use uncached memory for
//       everything, this doesn't actually work:
//        1. data at address X is written to GPU L2 (but not flushed to DRAM)
//        2. DMA transfer accessing X with uncached address will not see the data
typedef struct {
  uint32_t _0;
} bus_t;

// Memory layout looks like:
// 
// For the ARM CPU, we have:
//          0x0-0x1fffffff - DRAM
//   0x20000000-0x20ffffff - Peripherals
//
// For the GPU, we have:
//   0x00000000-0x3fffffff - COMPLETELY FORBIDDEN TO THE ARM (even over DMA)
//                           DO NOT USE THESE ADDRESSES!!!!!!!
//   0x40000000-0x5fffffff - DRAM (L2 cache coherent, allocating)
//   0x7e000000-0x7fffffff - Peripherals
//   0x80000000-0x9fffffff - DRAM (L2 cache coherent, non-allocating)
//   0xC0000000-0xDfffffff - DRAM (uncached / cache bypsas)
enum {
  BUS_DRAM_BASE = 0x40000000,
  
  // ARM-visible peripherals base address
  ARM_PERI_BASE = 0x20000000,
  // Peripheral bus mask
  BUS_PERI_BASE = 0x7e000000,

 // Used for sanity checking
  ARM_PERI_END = 0x21000000,
  BUS_PERI_END = 0x80000000,
};

static const inline _Bool is_arm_dram_addr(uintptr_t addr) {
  return addr < ARM_PERI_BASE;
}

static const inline _Bool is_arm_peri_addr(uintptr_t addr) {
  return addr >= ARM_PERI_BASE && addr < ARM_PERI_END;
}

static const bus_t arm_to_bus(uintptr_t addr)
{
  if (addr < ARM_PERI_BASE) {
    addr |= BUS_DRAM_BASE;
    return (bus_t){addr};
  } else {
    assert(is_arm_peri_addr(addr));
    // todo("implement this!");
    addr -= ARM_PERI_BASE;
    addr += BUS_PERI_BASE;
    return (bus_t){addr};
  }
}

static const inline void *bus_to_arm_ptr(bus_t b)
{
  // todo("implement this!");
  if (b._0 > 0x7e000000) {
    return (void*)(b._0 - 0x7e000000) + 0x20000000;
  } else {
    return (void*)(b._0 & ~0x40000000);
  }
}

// DMA Control block (p.40)
//
// NOTE:
//  - CRUCIAL: control blocks must be 32-byte aligned
struct {
  // Transfer Information (p.50-52)
  volatile uint32_t TI;
  // Source and destination addresses
  //
  // IMPORTANT: These are *bus addresses*; see docs for `bus_t` above.
  volatile bus_t SRC_ADDR, DST_ADDR;
  // Transfer length.
  //
  // When in 1D mode (i.e. normally) this is a 30-bit unsigned integer dictating the number of
  // bytes that will be transferred.
  //
  // When in 2D mode:
  //  - YLEN: bits 29:16 number of 'rows' to transfer, minus one
  //  - XLEN: bits 15:0 number of bytes to transfer per row
  volatile uint32_t TXFR_LEN;
  // 2D-mode stride.
  //
  // When in 2D mode:
  //  - D_STRIDE: bits 31:16 signed destination stride
  //  - S_STRIDE: bits 15:0 signed source stride
  //
  // CRUCIAL: Note that the stride is applied _after_ each row:
  // So if YLEN=1,XLEN=1,D_STRIDE=4,S_STRIDE=4, then the copies will be at indices 0 and 5, not 0
  // and 4.
  volatile uint32_t STRIDE;
  // Next CB address. Note that like SRC and DST, this is also a `bus_t`.
  volatile bus_t NEXT_CB;

  volatile uint32_t _ignore0, _ignore1;
} typedef __attribute__((aligned(32))) cb_t;

static inline _Bool is_cb_aligned(cb_t *cb) {
  return (uintptr_t)cb % 32 == 0;
}

// Bits in the TI (Transfer Information) field of the control block.
enum {
  _2DMODE = 1 << 1,
  DST_INC = 1 << 4,
  DST_INC_WIDE128 = 1 << 5,
  SRC_INC = 1 << 8,
  SRC_INC_WIDE128 = 1 << 9,

  WAITS_OFFSET = 21,
  WAITS_MAX = 0x1f,
};

// Bits in the TXFR_LEN field
static inline uint32_t _2d_dst_stride(uint16_t dst_stride) {
  assert(dst_stride < 0x3fff);
  return ((uint32_t)dst_stride) << 16;
}
static inline uint32_t _2d_src_stride(uint16_t src_stride) {
  return (uint32_t)src_stride;
}

static inline cb_t cb_mk(bus_t dst, bus_t src, size_t len) {
  cb_t cb = {};
  __builtin_memset(&cb, 0, sizeof cb);
  cb.TI = DST_INC|SRC_INC;

  if(len % 32 == 0)
    cb.TI |= DST_INC_WIDE128 | SRC_INC_WIDE128;

  cb.SRC_ADDR = src;
  cb.DST_ADDR = dst;

  cb.TXFR_LEN = len;
  return cb;
}

// =================================================================================================
// SECTION: BUS ADDRESS MANIPULATION
// =================================================================================================

enum cb_field {
  FLD_SRC,
  FLD_DST,
  FLD_LEN,
  FLD_NXT,
};
static const inline uint32_t cb_field_offset(enum cb_field f) {
  switch (f) {
  case FLD_SRC: return __builtin_offsetof(cb_t, SRC_ADDR);
  case FLD_DST: return __builtin_offsetof(cb_t, DST_ADDR);
  case FLD_LEN: return __builtin_offsetof(cb_t, TXFR_LEN);
  case FLD_NXT: return __builtin_offsetof(cb_t, NEXT_CB);
  default: __builtin_unreachable();
  }
}

// Convert ARM pointer to bus_t.
static const inline bus_t bus(volatile void *ptr) {
  return arm_to_bus((uintptr_t)ptr);
}
// Convert ARM pointer to bus_t, and add byte_offset.
static const inline bus_t bus_(volatile void *ptr, int32_t byte_offset) {
  return arm_to_bus((uintptr_t)ptr + byte_offset);
}

// Add `byte_offset` to `base`.
static const inline bus_t bus_add(bus_t base, int32_t byte_offset) {
  return (bus_t){._0 = base._0 + byte_offset};
}

// Add `cb_offset` control block lengths to `base`.
//
// For example:
// ```
// cb_t blocks[2];
// bus_t base = bus(&blocks[0]->TXFR_LEN);
// bus_t addr = bus_rel_cb(base,+1);
// ```
// addr will be the 0th byte of the length field of the 2nd block in `blocks`.
static const inline bus_t bus_rel_cb(bus_t base, int32_t cb_offset) {
  return bus_add(base, cb_offset * sizeof(cb_t));
}

// The address of the specified `field` (plus `offset_in_field`) in the control block `cb_offset`
// control blocks away from `base`.
//
// For example:
// ```
// cb_t blocks[2];
// bus_t base = bus(blocks);
// bus_t addr = bus_rel_fld(base,+1,FLD_SRC,+1);
// ```
// addr will be the +1th byte of the source address field of the 2nd block in `blocks`.
static const inline bus_t bus_rel_fld(bus_t base,
                                      int32_t cb_offset,
                                      enum cb_field field,
                                      uint32_t offset_in_field)
{
  // this operation only makes sense when base is a control block.
  assert(base._0 % 32 == 0);
  // fields in the control block are only 4 bytes
  assert(offset_in_field < 4);
  return bus_add(base, cb_offset * sizeof(cb_t) + cb_field_offset(field) + offset_in_field);
}

// Check if two bus addresses are equal.
static const inline _Bool bus_eq(bus_t a, bus_t b) { return a._0 == b._0; }

// =================================================================================================
// SECTION: ACTUAL DMA STRUCTURES
// =================================================================================================

// the DMA channel itself: p39, 
typedef struct {
    // all addrs must be manually translated or will get tricky
    // errors (see note below on l2, non-allocating)
    volatile uint32_t   
                CS;         //  0: control and status (p47)
    volatile bus_t
                CB_ADDR;    //  4: control block address (p50)

    volatile uint32_t   
                TI,         //  8: transfer info: lots of fields (p50)
                SRC_ADDR,   //  c: src addr: updated during dma p53
                DST_ADDR,   // 10: dst addr: updated during dma p53
                TXFR_LEN,   // 14: transfer in bytes p53
                ignore0;    // 18
    
    volatile bus_t
                NEXT_CB;    // 1c: note this must be a bus addr.

    volatile uint32_t
                DEBUG;      // 20
} dma_ch_t;

_Static_assert(offsetof(dma_ch_t, CS) == 0,       "Bad offset");
_Static_assert(offsetof(dma_ch_t, CB_ADDR) == 4, "Bad offset");
_Static_assert(offsetof(dma_ch_t, TI) == 8, "Bad offset");
_Static_assert(offsetof(dma_ch_t, SRC_ADDR) == 0xc, "Bad offset");
_Static_assert(offsetof(dma_ch_t, DST_ADDR) == 0x10, "Bad offset");
_Static_assert(offsetof(dma_ch_t, TXFR_LEN) == 0x14, "Bad offset");
_Static_assert(offsetof(dma_ch_t, NEXT_CB) == 0x1c, "Bad offset");
_Static_assert(offsetof(dma_ch_t, DEBUG) == 0x20, "Bad offset");

_Static_assert(offsetof(cb_t, TI) ==  0x0,       "Bad offset");
_Static_assert(offsetof(cb_t, SRC_ADDR) == 0x4, "Bad offset");
_Static_assert(offsetof(cb_t, DST_ADDR) == 0x8, "Bad offset");
_Static_assert(offsetof(cb_t, TXFR_LEN) == 0xc, "Bad offset");
_Static_assert(offsetof(cb_t, STRIDE) == 0x10, "Bad offset");
_Static_assert(offsetof(cb_t, NEXT_CB) == 0x14, "Bad offset");

static inline void
dma_detect_errors(dma_ch_t *r) {
    // p55
    let d = r->DEBUG;
    if(d & (1 << 2))
        panic("read error!\n");
    if(d & (1 << 1))
        panic("FIFO error!\n");
    if(d & (1 << 0))
        panic("READ NOT SET error!\n");

    let cb = r->CS;
    if((cb>> 8)&1)
        panic("DMA error set\n");
    if((cb>> 6)&1)
        panic("waiting for last write to be receied\n");
    if((cb>> 5)&1)
        panic("paused by DREQ state\n");

    if((cb>>28)&1)
        panic("waiting for outstanding writes?\n");

    return;
}

static inline int dma_active(dma_ch_t *dma) {
    // if active=0, we are done.
    return (dma->CS&1) != 0;
}

static inline int dma_done(dma_ch_t *dma) {
    // we aren't sure what they were doing, so 
    // use dev barrier
    dev_barrier();

    if(!dma_active(dma)) {
        assert(dma->CB_ADDR._0 == 0);
        return 1;
    }

    // not done: so check for errors
    dma_detect_errors(dma);
    return 0;
}

// initialize DMA channel <ch>.  see p41.  
//
// NOTE: while there are 15 DMA channels we can't use 
// many of them --- DMA is useful, so it turns out 
// different periphs are using it [not mentioned in 
// datasheet]!
//
// Seems to be avail: 4, 5, 8, 9, 10.
// Good:
// - Individual IRQ lines
// - Not used by firmware
// - No special restrictions
// - Full DMA capabilities
// Can also use 11, 12, 13, 14 but they have more 
// limits.
static inline dma_ch_t *
dma_init(unsigned ch) {
    enum { DMA_BASE = 0x20007000 };

    uint32_t ch_addr;
    switch(ch) {
    case 4:  ch_addr =  0x400 + DMA_BASE; break;
    case 5:  ch_addr =  0x500 + DMA_BASE; break;
    case 8:  ch_addr =  0x800 + DMA_BASE; break;
    case 9:  ch_addr =  0x900 + DMA_BASE; break;
    case 10: ch_addr = 0xa00 + DMA_BASE; break;
    case 15: ch_addr = 0xf00 + DMA_BASE; break;
    default:
        panic("illegal ch=%d\n", ch);
    }

    // R-M-W DMA enable so we don't trash other channels.
    enum { DMA_ENABLE = DMA_BASE + 0xff0 };
    dev_barrier();
    OR32(DMA_ENABLE, 1<<ch);
    dev_barrier();

    // NOTE: <dma_ch_t> fields are <volatile>, so it's ok the pointer
    // is not.
    return (void*)ch_addr;
}

static inline void 
dma_initiate_raw(dma_ch_t *dma, cb_t *cb) {
    // CRUCIAL: we use memory barriers to make sure
    // make sure the writes to <cb> (from 
    // the caller) don't get pushed below the 
    // assignment.  
    //
    // [With that said: since the fields are currently
    // volatile, shouldn't matter.]
    gcc_mb();
    dev_barrier();

    enum {
        DMA_CS_START = 1,
        DMA_CS_RESET = 1<<31,
    };

    dma->CB_ADDR = bus(cb);
    dma->CS      = DMA_CS_START;       // activate.

    dev_barrier();
    gcc_mb();
}

// Run <cb> on DMA engine <dma>. Does device barriers 
// and (should use) error checking.
static inline void 
dma_initiate(dma_ch_t *dma, cb_t *cb) {

    if(dma_active(dma))
        panic("DMA is still running!\n");

    if(!is_cb_aligned(cb))
        panic("bad alignment for control block %x\n", cb);

    if(!is_arm_dram_addr((uintptr_t)cb))
        panic("cb should be a physical addr! <%x>\n", cb);

    dma_initiate_raw(dma,cb);
}

// Wait for the current DMA transfer running on <dma> 
// to complete.
//
// Returns 1 on success, 0 on timeout.
static inline int dma_wait(dma_ch_t *dma, int timeout) {
    while(timeout-- > 0)
        if(dma_done(dma))
            return 1;

    // you should get rid of this: we use for lab so can't miss 
    // timeout.
    panic("timed out!\n");

    // timeout.
    return 0;
}

// Run <cb> on DMA engine <dma> with <timeout>.
//
// Returns 1 if completed without errors, 0 otherwise.
//
// NOTE: always easy to make an async interface sync
static inline int 
dma_run(dma_ch_t *dma, cb_t *cb, int timeout) {
    dma_initiate(dma, cb);
    return dma_wait(dma, timeout);
}

