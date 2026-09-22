#ifndef __VM_IDENT0_H__
#define __VM_IDENT0_H__

#include "vm-ident.h"
#include "cache-support.h"

// see libvm0.0/includes/mem-attr.h for the different 
// cache definitions, based on ch6 page 6-15 in the 
// arm1176.
//
// included here for ease of use:
//
// caching is controlled by the TEX, C, and B bits.
// these are laid out contiguously:
//      TEX:3 | C:1 << 1 | B:1
// we construct an enum with a value that maps to
// the description so that we don't have to pass
// a ton of parameters.
//
// #define TEX_C_B(tex,c,b)  ((tex) << 2 | (c) << 1 | (b))
// typedef enum {
//    //                              TEX   C  B 
//    // strongly ordered
//    // not shared.
//
//    // there's a bunch of other options --- unclear 
//    // this is correct.   this is just "Strongly Ordered"
//    // i think we need to do non-shared device
//    // 0b0010, 0, 0
//    // this is strongly ordered
//    MEM_device     =  TEX_C_B(    0b000,  0, 0),
//    MEM_noshare_dev = TEX_C_B(    0b010,  0, 0),
//    MEM_share_dev   = TEX_C_B(    0b000,  0, 1),
//
//    // normal, non-cached
//    MEM_uncached   =  TEX_C_B(    0b001,  0, 0),
//
//    // write back no alloc
//    MEM_wb_noalloc =  TEX_C_B(    0b000,  1, 1),
//    // write back alloc
//    MEM_wb_alloc   =  TEX_C_B(    0b001,  1, 1),
//
//    // write through no alloc
//    MEM_wt_noalloc =  TEX_C_B(    0b000,  1, 0),
//
//    // NOTE: missing a lot!
//} mem_attr_t;

// turn on VM.  caching is controlled by the page table attribute
// associated with each region (code,  heap, the stacks and the
// BCM memory region).
void vm_on_cache(void) {
    kmap_t k = kmap_default(1);

    // this should be the slowest setting.
    uint32_t attr = MEM_uncached;

    // do write-back attribute.  try the others!
    attr = MEM_wb_alloc;

    k.code.attr         =
    k.heap.attr         =
    k.int_stack.attr    =
    k.stack.attr        = attr;

    // device memory is tricky since it's not really
    // memory (can spontenously change, multiple reads with
    // no intervening write can return different values).
    // (see the arm manual).  
    //
    // i think it can be one of the following:
    k.bcm.attr = MEM_device;    // 104 cycles
    k.bcm.attr = MEM_share_dev; // 98 cycles

    // turn on vm and (potentially) define exception 
    // handlers.
    //
    // not a great interface, sorry.  am rewriting.
    // it's a great extension to do your own using 
    // your pinned VM system!  
    k.init_runtime_p = 0;
    map_kernel2(&k);

    // are there other caches that we are missing?
    // XXX: i'm not sure what to do about L2.
    caches_all_on();
    assert(dcache_l1_is_on());
    assert(icache_is_on());
    assert(btc_is_on());
}

// turn on VM with caches off: used to narrow down 
// why performance changes.
void vm_on_cache_off(void) {
    // turn off all caches that we can
    caches_disable();

    // get the memory map of all the regions in the base pi 
    // program (defined by the libpi/memmap linker script
    // and the constants we use for the stack and interrupt 
    // stack in <rpi-constants.h>
    kmap_t k = kmap_default(1);

    // make all regions besides the bcm uncached.
    k.code.attr         =
    k.heap.attr         =
    k.int_stack.attr    =
    k.stack.attr        = MEM_uncached;

    // use the default device attributes.
    k.bcm.attr = MEM_device;

    k.init_runtime_p = 0;
    map_kernel2(&k);

    //staff_lockdown_print_entries("after code is on");
    assert(!dcache_l1_is_on());
    assert(!dcache_l2_is_on());
    assert(!icache_is_on());
    assert(!btc_is_on());

    // XXX: does the arm1176 not support?
    //assert(!dcache_wb_is_on());
}

#endif
