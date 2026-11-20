**RUN `flip-check/` FIRST (`cd flip-check && make`) TO SEE IF YOUR BOARD CAN DO THIS LAB — IF IT PRINTS 0 FLIPS, TEXT ME.**

## Rowhammer!

**UPDATE: THE SECOND PART WITH SPRAYING PAGE TABLE ENTRIES IS NOW A MINOR EXTENSION**

I was taking cs350s and it had some cool hardware hacks so I thought I would mention one! - agniv

DRAM stores each bit as a charge in a capacitor, in a grid of rows. Reading a row (called an ACTIVATE) can electrically disturb the rows next to it, and so the memory controller will correct for this every 32-64ms, dependng on the operating system. If you are able to "hammer" one row enough times inside of one of these refresh windows, a bit in a neighboring row can flip! 

Note that these attacks require the capacitors to be sufficiently small enough for these disturbances to reach each other, and as a result most of these attacks were developed from 2014-2016.

Also, beacuse this is hardware sensitive, results can vary by board. I am able to get up to 120 bit flips, but you might have more robust hardware. Though... [idk man, see this attack on nvidia gpus](https://www.tomshardware.com/pc-components/gpus/new-geforge-and-gddrhammer-attacks-can-fully-infiltrate-your-system-through-nvidias-gpu-memory-rowhammer-attacks-in-gpus-force-bit-flips-in-protected-vram-regions-to-gain-read-write-access).

## Physics-y bullshit I don't really understand IDK

Most of my understanding of this was pulled from Kim et al. "Flipping Bits in Memory Without Accessing Them: An Experimental Study of DRAM Disturbance Errors," ISCA 2014. 

A DRAM cell is a capacitor and one access transistor (a MOSFET). It represents a bit by being fully charged or fully discharded. Cells are in a grid where "a horizontal wordline connects all cells in the [row]" and a vertical bitline for the column (check [the wikipedia article](https://en.wikipedia.org/wiki/Dynamic_random-access_memory#Array_structures)). To read a row you "activate" it, i.e. "the row's wordline must be enabled by raising its voltage." 

Capacitors can leak on their own through what Kim says is "subthreshold leakage and gate-induced drain leakage," which are just fancy ways of saying that current doesn't really play nicely. That's why the memory controller refreshes DRAM, and DDR3 specifically refreshes every 64ms, specifically by issuing "8192 REF commands during 64ms, once every 7.7us," meaning every row gets corrected once per window. 

Our pi zero isn't DDR3, though, it's LPDDR2, and the LPDDR2 refresh window is ~32ms while the chip is <=85 C (it gets smaller as it heats up). The firmware fires a refresh every 7.78us ([`rpi-open-firmware/firmware/sdram.c` L170](https://github.com/librerpi/rpi-open-firmware/blob/032ffa1b10675b8a47d16b57847a799dfe94fe8e/firmware/sdram.c#L170), defines `tREFI = 3113` cycles at 400 MHz). So on this board you get a ~32ms window, which is less slack for us!

However, if you repeatedly toggle a DRAM row's wordline, it induces some voltage fluctuations which can make neighboring cells "leak charge at an accelerated rate." This can lead to a bit flip! Kim doesn't have a definitive reason (as far as I can tell, it's not really known?) but it's quite cool!

## Geometry

First, we need to establish what a "row" is. DRAM is organized as a 2D sheet of capacitors, organized by (rows, banks, columns). There are 8 different banks, each of which correspond to a row x col. See below for the addressing:

```
  addr: [ ........ row ........ | bank | ...... column ...... ]
         31                  15  14  12 11                   0
             32KB row stride      8 banks      4KB page/column
```

We need to be in the same bank but attack different rows, meaning that for two addresses that are 32 KB apart, the bank and col bits are identical but the rows have changed. Now we can attack!

(One bookkeeping note: everything above is in **byte** addresses, but the code indexes `uint32_t` **words** (`arena[i]`). So a byte offset is `i * 4`, and the 32 KB row stride is `ROW_STRIDE / 4 = 8192` words. Whenever the README says "32 KB," the code steps 8192 array slots.)

### random reverse engineer shit, feel free to skip

So where does the 32KB come from? This was mostly from testing out different strides, but there's some hinting around in firmware/datasheets online. The memory controller is told its layout by the three fields `banklow` / `rowbits` / `colbits` which changes depending on RAM size in [`sdram.c` L246-L253](https://github.com/librerpi/rpi-open-firmware/blob/032ffa1b10675b8a47d16b57847a799dfe94fe8e/firmware/sdram.c#L246-L253) and writes into the SDRAM controller's `SD_SB` register at physical `0x7ee00008`. The real bit layout of that register is in the reverse-engineered in [`sdc_ctrl.h` L154-L198](https://github.com/librerpi/rpi-open-firmware/blob/032ffa1b10675b8a47d16b57847a799dfe94fe8e/common/broadcom/bcm2708_chip/sdc_ctrl.h#L154-L198):

```
  SD_SB @ 0x7ee00008:  [ ... | REORDER:7 | BANKLOW:6..5 | EIGHTBANK:4 | ROWBITS:3..2 | COLBITS:1..0 ]
```

So `BANKLOW`, `ROWBITS`, and `COLBITS` are each 2-bit fields. The 512MB pi zero has `banklow=2, rowbits=2, colbits=2`, and the 8 banks come from the single `EIGHTBANK` bit (bit 4) being set. The bootloader has the following selector for BANKLOW, see the [`SDRAM_BANKLOW` config option](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html) and [EEPROM bootloader docs](https://github.com/raspberrypi/documentation/blob/master/documentation/asciidoc/computers/raspberry-pi/eeprom-bootloader.adoc) give us:

```
  BANKLOW                address layout
    0    {bank[3:0], row, column}            all bank bits at the top
    1    {bank[3:1], row, bank[0], column}
    2    {bank[3:2], row, bank[1:0], column}   <- SD_SB raw field reads 2
    3    {bank[3], row, bank[2:0], column}
    4    {row, bank[3:0], column}            all low  <- but our MEASURED shape
```

This is from the rasp pi 4/5 bootloader, so it seems innacurate to me, since we'd have BANKLOW=4 if we were dealing with the rasp pi 0. So there's a weird jump in how these addresses are stored (and I have no real idea why you'd have this?). Also I can't find anything on the col/row bits, so if anyone can find this stuff or prove the actual address mapping that would be really cool, as the BCM2835 datasheet doesn't have anything on the videocore mmu since it lives at `0x7ee..`.

Anyway, tl;dr, stride of 32kb.

## Hammering, LDR vs STR

To attack a given row, you have to "hammer" the adjacent rows as many times as you can within this window:

```
   row v-1   <-- aggressor   write
   row v     <-- VICTIM      (a bit flips in here)
   row v+1   <-- aggressor   write
```

I haven't really explained what the code looks like for a hammer, but we'll go through it real quick. There are two possible ways to interact with an address in DRAM, a load or a store. Intuitively, a load is signifigantly faster than a store, so doing a load (cached or uncached) should really just work. It doesn't though! The following takes about 94 cycles:

```
  hammer(a, b): // a, b = two rows in the SAME bank, different rows
        t = *a // uncached LDR → bus → DRAM ACTIVATE(row a)
        t = *b // uncached LDR → bus → DRAM ACTIVATE(row b)
```

This doesn't end up flipping any bits, so we can enable cache, which leads to it taking 13 cycles instead. After we've done one hammer, though, the code looks like the following:

```
  hammer(a, b):
        t = *a // cached LDR → a's L1 cache line (not DRAM!)
        t = *b // cached LDR → b's L1 cache line (not DRAM!)
```

So it already couldn't do a hammer even if we tried! So if we only enable icache and disable dcache, it jumps up to 142 cycles (its slower?! wtf?) and doesn't work this time around. The conclusion here is that *loads do not allow you to do rowhammer on the rasp pi 0*. This is OPPOSITE to the pseudocode from the original Kim paper, which loads two values from DRAM and then clears cache line (see pg. 3 of the original paper). I'll buy a boba for anyone that can come up with a good reason for this behaviour. 

So we need to do a store. This kind of matches one of the attacks from the literature, specifically [Rowhammer Attacks on the Raspberry Pi 3B+](https://par.nsf.gov/servlets/purl/10220639), which describes a hammer this way:

```
hammer(a, b, val):   // caches on
    *a = val         // STR → L1 hit
    *b = val         // STR → L1 hit
    clean_to_DRAM(a) // DCCMVAC → ACTIVATE(row a)
    clean_to_DRAM(b) // DCCMVAC → ACTIVATE(row b)
    dsb()            // DSB
```

They use an ARMv8 instruction called `dc cvac`, which stands for Data Cache Clean by Virtual Address to PoC. The ARMv6 equivalent is `mcr p15,0,*,c7,c10,1`. The reason you need to do this is when you do a STR with caches on, it stores into L1 cache so future loads are about 1 cycle. To get around this, the authors had to establish coherency. 

So far loads haven't worked, and we can further test to see how the board differs. If a cached load only fails because it hits L1, then forcing it back out to DRAM should still not flip if reads don't disturb on this board. Upon running a cached LDR with a cache
maintenance op between the two loads and watched the PMU `dcache_miss` counter:

| cached LDR + | dcache_miss/hammer | reaches DRAM? | cyc/hammer | flips |
| --- | --- | --- | --- | --- |
| `dc cvac` (clean only, `c7,c10,1`) | 0 | no, stays in L1 | 57 | 0 |
| `dc civac` (clean+invalidate, `c7,c14,1`) | 1 | yes, forced miss | 156 | 0 |
| `dc ivac` (invalidate only, `c7,c6,1`) | 1 | yes | 156 | 0 |

So you need to invalidate the cache line for it to hit DRAM, and even then the load will flip nothing. So a load will not work as a hammer for us...

However, the following code with icache turned on or off just works on the pi zero!

```
    hammer(a, b, val):
        *a = val // touches DRAM
        *b = val // touches DRAM
```

This takes about 109 cycles REGARDLESS of being cached or not (if icache is on and dcache is off, there's some weirdness, I think it depends on how you start it)? And this ends up giving me a couple of bit flips reliably, which is crazy. (We also don't even need to do a dsb lol, will get to that)

### A quick explanation

Why do we not care about L1? The ARM1176 manual in section 7.2 states that when you do a store, two things can happen, a write-back or a write-allocate. If your store hits a cached line, a write-back will update the cached copy and mark it dirty (i.e. it doesn't match its actual address). A write-allocate happens if the cache misses, and it will bring the line into the cache before it updates it.

The Pi 3B+ will (generally) allocate L1 lines on both read+write misses. Thus two repeated stores can settle into L1 hits, meaning that the authors needed to do a `DC CVAC` twice to clean the dirty lines to persist the write into DRAM. This is documented in the Cortex-A53 manual in section 6.2.5 under "Read allocate mode," see "normally allocates a cache line on either a read miss or a write miss."

However, the Pi Zero *will not allocate L1 lines on a store miss*! This is what the line "only read allocation is supported" in the documentation (ARM1176 manual, section 7.2) means! So, for a cacheable write-back mapping, we have the following behaviour:

| Access | Line absent from L1 | Line already in L1 |
| --- | --- | --- |
| `LDR` | Fetch and cache the line | Read the cached copy |
| `STR` | Send the write onward through the write buffer; don't install an L1 line | Update the cached copy and mark it dirty |

If `a` and `b` start absent from L1, this can just keep missing:

```text
repeat:
    *a = val    // write goes onward, but a still isn't installed in L1
    *b = val    // same for b
```

Also, as a side note, DSB doesn't actually flush L1, since it really just needs to make sure that instructions are done in order (it doesn't care about coherency). It's also okay that we don't include it, off of vibes this is because we really just care about it doing a store rather than the order it does it in. 

## Actual Lab Content

There are three parts to Rowhammer here: **Attacking** (flip a bit), **Spraying** (turn flips into a real exploit), and **Defending** (stop it).

### Attacking

First, as per usual, DRAM starts from physical address 0, and for testing, I attacked a 16 MB region that was 64 MB into DRAM as to avoid clobbering the actual running code, though it might be funny to attack it and see what happens! I enabled virtual memory and caching (though I guess you don't have to), and let the attacked region be write-back & cache-allocating:

```c
vm_on_cache();

ksec_t k = ksec_default(ARENA_BASE, dom_kern);
k.n_mb = ARENA_MB;
k.attr = MEM_wb_alloc;
ksec_map(5, &k); // 0-4 used
dsb();
```

After that, we will need to know about the geometry of DRAM, but we only really need to know that we move up and down by 32KB. As a test, I set every even-indexed row to all zeros and every odd row to all ones. Now, it's hammer time. For each all-zero row, do a bunch of double-sided store hammers on its two all-ones neighbors within the reset window period (I just do a bunch of them, it doesn't really matter). After doing all of this, we check which bits changed from the pattern we set up.

I don't attack all of them just b/c the store based attack requires you overwrite what could have been an attacked row. You also can't do a load in between just because it would load this to cache, so. I get ~100 flips on a cold board.

You could do fancier, more thorough things if you want, like hammering the zero rows too to catch rarer `1->0` flips (I can't get these to happen), or hammer *every* row and count both directions. For that last one I think you should split the sweep into two passes by parity (hammer the even victims, count, then the odd).

You can test this with any known pattern at the beginning, some bits are easier to flip from 0 to 1 compared to 1 to 0, etc. Let me know what's the maximum number of bit flips you can get!

### Spraying

So now you can flip some bits! But since we don't get to pick which bit actually flips, how is this an actual attack? We get around this by a "spraying" attack. What we can do is spray a bunch of page tables and hope that we flip a bit inside of the address that it points to. After that happens, we now have access to a different region of memory.

I've set up some starter code inside of [`1-spray/spray.c`](1-spray/spray.c). First, we set up an enclave with a secret passcode. As the attacker, I set up the same region (16 MB) with page tables that point to the `F_att` from the enclave. Then we hammer our own shit (we let some rows be all ones and hammer these!) and check after if we're able to access the secret frame! Once we find a descriptor thats been updated, we can install it into a virtual address we own, dereference it, and out comes the secret.

Our sprayed descriptor points at section `0x60`, and the frame is just the descriptor's top 12 bits, so a single `0->1` flip up there can only ever turn `0x60` into one of `{0x61, 0x62, 0x64, 0x68, 0x70, 0xE0, 0x160}` (`0x60` is `0b0110_0000`, so flip any single zero-bit that keeps the frame under 512 MB and you get exactly those). So, assuming the enclave plants its secret in every one of those frames ahead of time (kind of gratifying but ayo chill in n out is fire), any single base-bit flip, in any of the ~2 million descriptors we sprayed, can redirect onto a secret. Checking whether a flipped frame is one of these with `is_secret` is given, since it's just a membership test in that set. Pulling the frame out of the descriptor word with `desc_base_sec` is from reading the `sec_base_addr` field of the `fld_t` section descriptor in `armv6-vm.h`. For something to redirect to a secret, it must have a valid section descriptor and have its frame pointing to a secret.

This is the rough pseudocode:

```
spray:       paint every arena word   ->  descriptor (even) rows = SANDBOX_DESC (-> F_att),
                                       odd rows = 0xffffffff (the aggressors)
hammer:      for each descriptor row  ->  pound its two all-ones neighbors, ITERS times
hunt:        for each sprayed word d  ->  if redirects_to_secret(d), you found a redirect!
dereference: probe_read(desc_base_sec(d))  ->  map that frame to a VA you own, deref -> secret
```

Once we have a redirected descriptor, we need to use it. This can be done by loading it into a virtual address we own (see `live_map`), and then dereference it to get the actual value. 

```
PWNED: arena word 0x103e4 flipped 0x6001c22 -> 0x6101c22 (frame 0x60 -> 0x61)
secret key: "in-n-out-is-awesome"
```

### Defending (the last step)

Okay, so how do you stop this? What we can do is artifically reduce the amount of time it takes to refresh from 32ms, either via timer interupts or in this case, the DMA. What this part of the lab is going to do is set up a DMA control block that copies a clean copy over the attacked arena, and self loop. This way the DMA can recharge the rows! 

```
golden = clean committed copy of the region        // given
start_refresh:  cb = copy golden -> arena (uncached alias);  cb.NEXT_CB = cb   // self-loop
                dma_init(ch);  dma_initiate(cb)     // kick once -> loops forever
run_row()                                           // YOUR 0-hammer attack, now defended
stop_refresh:   dma->CS = 1u<<31                    // halt, THEN trust the counts
```

Note that the DMA lives on the VideoCore side and has a different address map. Specifically, we need to bitwise-OR with `0xC0..` to bypass the cache (the diagram in the datasheet explains this!!, also `dma-impl.h:52`). Straight off my pi:

```
your 0-hammer attack, no refresh    -> ~100 flips
your 0-hammer attack + DMA refresh  ->   0 flips
```

## Extensions

Once the three parts work, here's some things that might be cool:

- **If you can describe the full address map that would be sick!** This was reversed with claude by varying the stride until there were flips at all, which feels kind of bullshit.
- **Do a 1 -> 0 flip!**
  - No clue if this is possible.
- **I'll buy a boba if you have a real explanation for why load doesn't work on this board,** I really have no clue man.
  - Increasing the # of loads might help? I have no clue. 
- Do a **flip on a live page table** (i.e. the page table the MMU would be walking right now). 
  - A live page table is the one the MMU is walking right now to translate your accesses (the `live_pt` in `pte-vm.h` is exactly that). In Spraying you flipped an inert *copy* and then installed it, but here you're flipping a descriptor that's already in the table the hardware is actively using, so the redirect takes effect on the very next load/store, avoiding no install step at all.
  - Rough setup: map a big window of VAs (a few hundred MB) through descriptors that live in the hammerable arena, point them all at your own benign frame, hammer, then just dereference the window. Whichever descriptor flipped now silently reads a frame you were never granted, mid-stride, while the MMU walks it. (You need an in-DRAM table for this, which `pte-vm.h` already builds.)

happy hacking!

-agniv
