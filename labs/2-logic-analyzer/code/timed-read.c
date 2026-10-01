// [NOTE: this code should remind you alot about 1-fast-dev-int.
// its good practice to speed run again.]
//
// Simple code to measure the time it takes to record timed
// reads.
//
// you can use these to make a digital scope, or make bit banged
// digital protocols.  dual of observing for measurement and 
// observing for information.
//
// The file "interrupt-asm.S" has the support assembly code.
//
// You should put a loopback jumper between <in_pin> and 
// <out_pin> (definitions below).
//
// Lab: make this code much faster / accurate.
#include "rpi.h"
#include "timer-interrupt.h"
#include "cycle-count.h"
#include "vector-base.h"

// raw timed reads: gives the cycle count (absolute) and the raw
// level.  post process this into a timed log(?)
typedef struct { 
    uint32_t lev;   // raw lev
    uint32_t cyc;   // raw cycles
} timed_read_t;

// not sure if you should check the end.
// probably should just do a watchpoint.
enum { MAX_READS = 128 };
static timed_read_t t_reads[MAX_READS], *t_end = &t_reads[MAX_READS];
static timed_read_t * volatile tr = &t_reads[0];

static inline timed_read_t * volatile read_cur(void) {
    return tr;
}

// Can change these pins to whatever you want.  
// 
// NOTE: some pin numbers let the compiler generate faster 
// code b/c it can load derived constants in fewer instructions
// [useful side quest: write some code to check this claim!]
enum { out_pin = 26, in_pin = 27 };

static void int_init(void) {
    tr = &t_reads[0];
}

// a bit fancier than the device interrupt since we 
// are recording a log.
__attribute__((aligned(32))) void int_vector(void) {
    uint32_t cycle = cycle_cnt_read();

    // we don't know what the user code was doing
    dev_barrier();
    uint32_t v = gpio_read(in_pin);

    *tr++ = (timed_read_t) { .cyc = cycle, .lev = v<<in_pin };
    if(tr > t_end)
        panic("overflow\n");

    // gpio and event clear may need device barrier?
    dev_barrier();

    gpio_event_clear(in_pin);

    // we don't know what the user code was doing
    dev_barrier();
}

// driver that triggers and measures the interrupts
// caused by writing to GPIO <pin>.
void test_cost(unsigned pin) { 
    int_init();
    // initial state.
    assert(gpio_read(in_pin) == 0);

    float sum = 0;
    uint32_t c,e;
    enum { N = 20 };

    for(int i = 0; i < N/2; i++) {
        // measure the cost of a rising edge interrupt.
        // by reading cycle counter and spinning until the
        // rising edge count increases (i.e., an interrupt
        // occured).
        let r = read_cur();
        asm volatile(".align 5");
        c = cycle_cnt_read();
        gpio_set_on(pin);
        while(r == read_cur())
            ;
        e = cycle_cnt_read();
        // compute what the level was
        let lvl = (r->lev >> in_pin) &1;

        output("%d: rising  [lev=%d]\t= %d total cycles [%d until int ran]\n", 
            i*2, 
            lvl,
            e-c,
            r->cyc -c);
        sum += e-c;
        if(lvl != 1)
            panic("LOG error: level not 1 on a rising interrupt\n");

        // measure the cost of a falling edge interrupt.
        // by reading cycle counter and spinning until the
        // falling edge count increases (i.e., an interrupt
        // occured).
        r = read_cur();
        asm volatile(".align 5");
        c = cycle_cnt_read();
        gpio_set_off(pin);
        while(r == read_cur())
            ;
        e = cycle_cnt_read();

        // compute what the level was
        lvl = (r->lev >> in_pin) &1;

        output("%d: falling [lev=%d]\t= %d total cycles [%d until int ran]\n", 
            i*2+1, lvl, e-c, r->cyc - c);
        sum += e-c;
        if(lvl != 0)
            panic("LOG error: level not 0 on a falling interrupt\n");
    }
    output("ave cost = %f\n", sum / 20);
}

// all this is the same as 1-fast-dev-int.
void notmain() {
    //*****************************************************
    // 1. setup pins and check that loopback works.
    gpio_set_output(out_pin);
    gpio_set_input(in_pin);

    // make sure there is a jumper b/n <in_pin> and <out_pin>
    gpio_write(out_pin, 1);
    if(gpio_read(in_pin) != 1)
        panic("connect jumper from pin %d to pin %d\n", 
                                    in_pin, out_pin);
    gpio_write(out_pin, 0);
    if(gpio_read(in_pin) != 0)
        panic("connect jumper from pin %d to pin %d\n", 
                                    in_pin, out_pin);

    //*****************************************************
    // 2. setup interrupts in our standard way.
    extern uint32_t default_vec_ints[];

    // setup interrupts.  you've seen this code
    // before.  (we're assuming ints are off.)
    dev_barrier();
    PUT32(IRQ_Disable_1, 0xffffffff);
    PUT32(IRQ_Disable_2, 0xffffffff);
    dev_barrier();
    vector_base_set(default_vec_ints);

    // setup interrupts on both rising and falling edges.
    gpio_int_rising_edge(in_pin);
    gpio_int_falling_edge(in_pin);

    // the above sample twice triggering (for a stable
    // signal) --- in theory these should be faster.
    // gpio_int_async_rising_edge(in_pin);
    // gpio_int_async_falling_edge(in_pin);

    // clear any existent GPIO event so that we don't 
    // get a delayed interrupt.
    gpio_event_clear(in_pin);

    output("About to go live.\n");
    enable_interrupts();
    // now we are live!

    //*****************************************************
    // 3. run the test.

    // leave this off initially so its easier to see the effect
    // of speed improvements.
    // caches_enable();

    test_cost(out_pin);
    output("Done!\n");
    return;
}
