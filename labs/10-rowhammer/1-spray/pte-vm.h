#ifndef __PTE_VM_H__
#define __PTE_VM_H__

// given: the armv6 VM scaffolding for the spray (in-DRAM page table + enclave).

#include "vm-enable.h"
#include "armv6-vm.h"           // fld_t: a 1MB section descriptor

#define MB_             (1024u * 1024u)
#define PT_ENTRIES      4096                      // 4GB / 1MB: a full first-level table
#define PT_ALIGN        (16 * 1024)               // the table must be 16KB-aligned
#define SECTION_ADDR(s) ((uint32_t)(s) << 20)     // section number -> physical address

#define F_ATT     0x6000000u    // the attacker's own frame (section 0x60)
#define PROBE_VA  0xC000000u    // spare VA we point at a flipped frame to read it

// the secret lives in all of these: each is one 0->1 bit flip from 0x60, inside 512MB.
static const uint32_t secret_sec[] = { 0x61, 0x62, 0x64, 0x68, 0x70, 0xE0, 0x160 };
enum { N_SECRET = sizeof secret_sec / sizeof secret_sec[0] };

uint32_t SANDBOX_DESC;          // the descriptor we spray: maps -> F_att
static fld_t *live_pt;          // the page table the MMU walks (in DRAM)

// a valid section descriptor?
static inline int desc_is_section(uint32_t w) { return (w & 0b11) == 0b10; }

// build one section descriptor: map a 1MB VA onto physical <pa>, cache attrs <attr>, domain <dom>.
static fld_t section(uint32_t pa, mem_attr_t attr, unsigned dom) {
    return (fld_t){
        .tag           = 0b10,                 // 1MB section
        .B             = bit_get(attr, 0),     // B / C / TEX = the cache attributes
        .C             = bit_get(attr, 1),
        .TEX           = bits_get(attr, 2, 4),
        .AP            = 0b11,                  // read/write
        .domain        = dom,
        .sec_base_addr = pa >> 20,             // the frame -- what a bit flip moves
    };
}

// the same descriptor as a raw 32-bit word.
static uint32_t desc_word(uint32_t pa, mem_attr_t attr, unsigned dom) {
    fld_t f = section(pa, attr, dom);
    uint32_t word;
    memcpy(&word, &f, sizeof word);
    return word;
}

// install a descriptor into the live table, then sync the MMU so it takes effect.
static void live_map(uint32_t va, uint32_t pa, mem_attr_t attr, unsigned dom) {
    live_pt[va >> 20] = section(pa, attr, dom);
    mmu_sync_pte_mods();
}

// drop a string into a physical section, mapped uncached so reads go straight to DRAM.
static void plant(uint32_t sec, const char *str) {
    uint32_t pa = SECTION_ADDR(sec);
    live_map(pa, pa, MEM_uncached, dom_kern);
    strcpy((char *)pa, str);
}

// one-time setup: build the in-DRAM table, switch the MMU onto it, map the arena
// cached (hammering needs that), and lay down the enclave's frames.
static void pte_lab_setup(uint32_t arena_base, uint32_t arena_mb, const char *secret) {
    live_pt = kmalloc_aligned(PT_ENTRIES * sizeof *live_pt, PT_ALIGN);

    for (uint32_t sec = 0; sec < 512; sec++)            // low 512MB: cached identity map
        live_pt[sec] = section(SECTION_ADDR(sec), MEM_wb_alloc, dom_kern);
    for (uint32_t sec = 0x200; sec < 0x203; sec++)      // the BCM peripheral window (device)
        live_pt[sec] = section(SECTION_ADDR(sec), MEM_share_dev, dom_bcm);

    staff_mmu_set_ctx(0x5050, default_ASID, live_pt);   // run on our table from here on
    mmu_sync_pte_mods();

    for (uint32_t a = arena_base; a < arena_base + arena_mb * MB_; a += MB_)
        live_map(a, a, MEM_wb_alloc, dom_kern);         // the arena: cached + hammerable

    SANDBOX_DESC = desc_word(F_ATT, MEM_uncached, dom_kern);   // the descriptor we spray

    plant(0x60, "sandbox: nothing to see here");        // F_att: a benign decoy
    for (unsigned i = 0; i < N_SECRET; i++)             // the real secret in every neighbor frame
        plant(secret_sec[i], secret);
    dsb();
}

#endif
