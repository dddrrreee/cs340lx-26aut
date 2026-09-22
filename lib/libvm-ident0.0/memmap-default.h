#ifndef __MEMMAP_DEFAULT_H__
#define __MEMMAP_DEFAULT_H__
// this file has the default values for how memory is laid out
// in our default <libpi/memmap>: where the code, data, heap, 
// stack and interrupt stack are. 
//
// there's other ways to do this but we try to make it more 
// transparent by making it as primitive as possible.
//
// XXX: probably should add a pmap structure that tracks
// the attributes used for a MB.  this lets us do identity
// mapping easily, and also check for compatibility in terms
// of caching, etc.

// the symbols 
#include "memmap.h"
#include "pinned-vm.h"

// we put all the default address space enums here:
// - kernel domain
// - user domain
// - layout.

#define MB(x) ((x)*1024*1024)

// default domains for kernel and user.  not required.
//
// do we actually use this?
enum {
    dom_kern = 1, // domain id for kernel
    dom_user = 2,  // domain id for user
    dom_trap = 3,  // domain used for trapping
    dom_heap = 4,
    dom_bcm  = 0,

    // get rid of these.

    // setting for the domain reg: 
    //  - client = checks permissions.
    //  - each domain is 2 bits
    dom_bits = DOM_client << (dom_kern*2) 
             | DOM_client << (dom_user*2),

    // this only has the kernel domain: 
    // this will trap any heap acces.
    trap_access     = dom_bits,
    no_trap_access  = trap_access
                    |  DOM_client << (dom_trap*2)
};

enum { 
    // default no user access permissions
    no_user = perm_rw_priv,
    // default user access permissions
    user_access = perm_rw_user,
};

// the default asid we use: not required.
// recall that ASID = 0 is reserved as 
// a scratch register used when switching 
// address spaces as per the ARM manual.
enum {
    default_ASID = 1
};


// These are the default segments (segment = one MB)
// that need to be mapped for our binaries so far
// this quarter. 
//
// these will hold for all our tests today.
//
// if we just map these segments we will get faults
// for stray pointer read/writes outside of this region.
//
// big limitation: the fact that binaries start at
// 0x8000 means that we can't leave 0 unmapped when
// using 1mb segments.
//
// NOTE: we could derive a bunch of these from 
// linker symbols and there should be more
// alignment checking.  
enum {
    // code starts at 0x8000, so map the first MB
    //
    // if you look in <libpi/memmap> you can see
    // that all the data is there as well, and we have
    // small binaries, so this will cover data as well.
    //
    // NOTE: obviously it would be better to not have 0 (null) 
    // mapped, but our code starts at 0x8000 and we are using
    // 1mb sections (which require 1MB alignment) so we don't
    // have a choice unless we do some modifications.  
    //
    // you can fix this problem as an extension: very useful!
    SEG_CODE = MB(0),

    // as with previous labs, we initialize 
    // our kernel heap to start at the first 
    // MB. it's 1MB, so fits in a segment. 
    SEG_HEAP = MB(1),

    // if you look in <staff-start.S>, our default
    // stack is at STACK_ADDR, so subtract 1MB to get
    // the stack start.
    SEG_STACK = STACK_ADDR - MB(1),

    // the interrupt stack that we've used all class.
    // (e.g., you can see it in the <full-except-asm.S>)
    // subtract 1MB to get its start
    SEG_INT_STACK = INT_STACK_ADDR - MB(1),

    // the base of the BCM device memory (for GPIO
    // UART, etc).  Three contiguous MB cover it.
    SEG_BCM_0 = 0x20000000,
    SEG_BCM_1 = SEG_BCM_0 + MB(1),
    SEG_BCM_2 = SEG_BCM_0 + MB(2),

    // we guarantee this (2MB) is an 
    // unmapped address.  
    //
    // XXX: should pull this from the active pmap.
    SEG_ILLEGAL = MB(2),
};


#if 0
// default kernel attributes
static inline pin_t dev_attr_default(void) {
    return pin_mk_global(dom_kern, no_user, MEM_device);
}
// default kernel attributes
static inline pin_t kern_attr_default(void) {
    return pin_mk_global(dom_kern, no_user, MEM_uncached);
}

static inline uint32_t trap_dom(void) {
    return dom_trap;
}
#endif

// sets up a identity VM mapping of the entire address 
// space based on our default <memmap>.
//
// returns the next free MB available.
//
// note: 
//   - really need to figure out a better approach
//     for mapping the heap.  
//   - more generally: this doesn't really compose well.
//     probably should have a structure with <heap>,
//     <code>, <data>, <stack> <bcm> fields
//     with client provided (attribute, perm, and domains) 
//     for each.  fill the structure with reasonable defaults 
//     and let clients override.
//
//     could also have the base addr and size so that <memmap>
//     can fill in.  better than current approach...
int vm_map_everything(uint32_t heap_mb);

// find a free MB section, allocate it, map it with
//  - domain <dom>
//  - permissions <perm>.  
// returns the address.
uint32_t mb_map(uint32_t addr, uint32_t dom, uint32_t perm);

// reserve a 1MB segment but do not map it.  
// if <addr>=0, reserves the next free MB.
// note: should allow <n_mb> parameter.
uint32_t mb_reserve(uint32_t addr);

// find a free segment starting from the low part of the addr
// space.  note: should provide a <n_mb> param.
uint32_t mb_find_free(void);

// kernel: identiy-mapped n*1MB section.  
// default:
//   perm = no_user
//   dom = unique to each different contig segment.
//   attr = MEM_uncached
//   1mb
//
// note: maybe we should state the size of the base page (64k, 1mb etc)
// and the number of base pages.
// otherwise if we have to map (say) 32mb we have to figure
// stuff out (is it aligned etc)
//
typedef struct kern_sec {
    uint32_t    addr;   // virtual address. physical addr is same.
    // this could just be the number of bytes and then we map it.
    uint8_t     n_mb;   // how many mb: should be 1 forall but bcm
    uint8_t     dom;    // mb>1 implies dom=0
    /* mem_perm_t */ uint32_t perm;   // permissions.
    /* mem_attr_t */ uint32_t attr;   // caching attributes.
} ksec_t;

// XXX: need to figure out if 16mb super can have dom != 0.
//
// default kernel attribute is uncached.  if you want speed,
// change this or override later (recall: arm1176 dcache needs 
// VM on)
static inline ksec_t ksec_default(uint32_t addr, uint8_t dom) {
    // should reserve the memory, right?
    assert(addr%MB(1) == 0);
    return  (ksec_t) {
        .addr   = addr,
        .n_mb   = 1,
        .dom    = dom,
        .perm   = no_user,
        .attr   = MEM_uncached
    };
}

// default: put bcm in single 16mb section.  
static inline ksec_t ksec_bcm(uint32_t addr, uint8_t dom) {
    assert(!dom);

    // must be supersection aligned for 16mb
    assert(addr%MB(16) == 0);
    return  (ksec_t) {
        .addr   = addr,
        .n_mb   = 16,
        .dom    = dom,
        .perm   = no_user,
        .attr   = MEM_device
    };
}

#include <stddef.h>


// entire kernel address space for pinned memory.
// this method does make iteration more annoying.
//
// note: the big problem with our approach is that it doesn't
// work well with us using different aggregations.  e.g., putting
// everything in a single 16mb page.  maybe we should just map
// things out exactly, with the attributes we want, and then
// do the aggregation later?
typedef struct kmap {
    // probably should seperate out the code, data, etc and then
    // the mapping actually figures stuff out.
    ksec_t  code,       // right now code and data share same map: fix!
            stack,      // should merge these: we don't need so much.
            int_stack,
            // should we have this seperate?
            // needs to figure out better.
            // can have a set of heap pointers.
            heap,
#define     kmap_last offsetof(struct kmap, bcm)
            bcm;        // bcm memory.

    uint32_t dom;       // literal value of dom reg.
    unsigned idx;       // pinned index.
    unsigned init_runtime_p:1; 
} kmap_t;

#define kmap_size_assert(n) \
    _Static_assert( kmap_last / sizeof(ksec_t) == n-1,\
       "padding or wrong number of entries!")

#if 0
#define kmap_size_assert(n) \
    _Static_assert(sizeof(kmap_t) / sizeof(ksec_t) == n, \
       "padding or wrong number of entries!")
#endif

#if 0
_Static_assert(sizeof(kmap_t) == sizeof(kmap_t)*5,
       "padding or wrong number of entries!");

// this is not recommended, but (1) the struct fields are
// the same type and (2) we verified there is no padding.
#define kmap_first(k)   &(k)->code
#define kmap_last(k)    &(k)->bcm
#define kmap_inc(e)    (e+1)
#define kmap_next(k,e)  (((e)==kmap_last(k)) ? 0 : (e+1))
#define kmap_apply(k,stmts) do {                            \
    for(let e = kmap_first(k); e; e = kmap_next(k,e)) {     \
        stmts;                                              \
    }                                                       \
} while(0) 

    let e = kmap_first(&k);
    let l = kmap_last(&k);
    uint32_t dom1 = 0;
    // end should be past the end?
    for(; e <= l; e = kmap_inc(e))
        dom1 |= 1<<e->dom;

    // a bit easier to make sure don't go too far.
    uint32_t dom2 = 0;
    for(let e = kmap_first(&k); e; e = kmap_next(&k,e))
        dom2 |= 1<<e->dom;
    assert(dom1==dom2);

    uint32_t dom3 = 0;
    kmap_apply(k, e, dom3 |= 1 << e->dom);
    assert(dom3==dom2);

#if 0
#define kmap_apply(V, k, args...) do { \
    V((k)->code, args);                \
    V((k)->stack, args);               \
    V((k)->int_stack, args);           \
    V((k)->heap, args);                \
    V((k)->bcm, args);                 \
} while(0)
#endif
#endif


kmap_size_assert(5);

#define kmap_apply(k, e_name, stmts) do {        \
    { let e_name = &k->code;        stmts; }     \
    { let e_name = &k->stack;       stmts; }     \
    { let e_name = &k->int_stack;   stmts; }     \
    { let e_name = &k->heap;        stmts; }     \
    { let e_name = &k->bcm;         stmts; }     \
} while(0)

static inline kmap_t kmap_default(unsigned heap_mb) {
    if(heap_mb != 1)
        panic("need to figure out domain for supersections\n");
        
    // NOTE: for pinned: we could throw the entire kernel in a 
    // 16mb segment, freeing up 6pins for user. probably should
    // do for 140e '26.
    return (kmap_t) {
        // we could trap on the normal stack if wanted,
        // and/or the data segment if it was seperate from
        // the code.  might be worth seperating these domains
        // out.
        .code       = ksec_default(SEG_CODE,        dom_kern),
        .stack      = ksec_default(SEG_STACK,       dom_kern),
        .int_stack  = ksec_default(SEG_INT_STACK,   dom_kern),
        .heap       = ksec_default(SEG_HEAP,        dom_heap),
        .bcm        = ksec_bcm(SEG_BCM_0,           dom_bcm),
        .init_runtime_p = 1,
    };
}

#include "full-except.h"

static inline void map_kernel1(kmap_t *k) {
    kmalloc_init_set_start((void*)k->heap.addr, k->heap.n_mb);
    full_except_install(0);

    uint32_t dom = 0;
    kmap_apply(k, e, dom |= DOM_client << (e->dom*2));
    pin_mmu_init(dom);

    unsigned idx = 0;

    // pin all the mappings.
    kmap_apply(k, e, {
            pin_t p = pin_mk_global(e->dom, e->perm, e->attr);
            if(e->n_mb != 1) {
                assert(e->n_mb == 16);
                p = pin_16mb(p);
            }
            pin_mmu_sec(idx++, e->addr, e->addr, p);
            assert(idx < 8);
        }
    );

    // we aren't using user processes or anythings so we
    // just claim ASID=1 as our address space identifier.
    enum { ASID = 1 };
    pin_set_context(ASID);

    // turn the MMU on.
    assert(!mmu_is_enabled());
    mmu_enable();
    assert(mmu_is_enabled());
}

// can extend this to have optional segments by
// using n_mb as a valid tag.
// can extend to have 2,3, ... mb segments just
// iterating and mapping them.  will have to pass in 
// <k> to update idx tho.
static inline uint32_t ksec_map(unsigned idx, ksec_t *e) {
    pin_t p = pin_mk_global(e->dom, e->perm, e->attr);
    if(e->n_mb != 1) {
        assert(e->n_mb == 16);
        p = pin_16mb(p);
    }
    pin_mmu_sec(idx, e->addr, e->addr, p);
    assert(idx < 8);
    return DOM_client << (e->dom*2);
}

// need to figure out the exception stuff.  do we pull this out entirely?
// also the kmalloc.  why is it it here?
static inline void map_kernel2(kmap_t *k) {
    // haven't done any initialization.
    assert(!k->idx);
    assert(!k->dom);
    assert(!mmu_is_enabled());


    // we need to really sort this out: this should be in the 
    // runtime setup?
    // actually: we can't do this.
    //
    // pass this as the option anyway.
    if(k->init_runtime_p) {
        void *h_start   = kmalloc_heap_start();
        void *h_end     = kmalloc_heap_end();

        if(!h_start)
            kmalloc_init_set_start((void*)k->heap.addr, MB(k->heap.n_mb));
        else {
            void *s = (void*)k->heap.addr;
            void *e = h_start + MB(k->heap.n_mb);

            if(s != h_start)
                panic("already initialized heap: start=%p, need=%p\n", h_start,s);
            if(h_end > e )
                panic("already initialized heap: end=%p, need=%p\n", h_end,e);
        }

        // not sure about this
        full_except_install(0);
    }

    // intialize MMU: probably should pull the dom out first.
    // see above: apply works.
    pin_mmu_init(~0);
    

    // should allow heap to have multiple disjoint mappings.
    uint32_t idx = 0, dom = 0;
    dom |= ksec_map(idx++, &k->code);
    dom |= ksec_map(idx++, &k->stack);
    dom |= ksec_map(idx++, &k->int_stack);
    dom |= ksec_map(idx++, &k->heap);
    dom |= ksec_map(idx++, &k->bcm);



    // set the domain register to computed result.
    domain_access_ctrl_set(dom);

    // i don't think this is needed.
    //
    // we aren't using user processes or anythings so we
    // just claim ASID=1 as our address space identifier.
    // since everythig is global this should be completely
    // irrelevent.  we just don't want it to be undefined.
    enum { ASID = 1 };
    pin_set_context(ASID);


    // turn the MMU on.
    mmu_enable();
    assert(mmu_is_enabled());

    // save these in case you add additional mappings.
    assert(idx < 8);
    k->idx = idx;
    k->dom = dom;
}

// disable mmu
static inline void unmap_kernel(kmap_t *k) {
    mmu_disable();
    mmu_sync_pte_mods();
#if 0
    k->idx = 0;
    k->dom = 0;
    k->init_runtime_p = 0;
#endif

    // probably should kill all the different
    // indexed mappings from 0 .. idx
}

static inline void remap_kernel(kmap_t *k) {
    mmu_disable();
    mmu_sync_pte_mods();

    k->idx = 0;
    k->dom = 0;
    k->init_runtime_p = 0;
    map_kernel2(k);
}

#endif
