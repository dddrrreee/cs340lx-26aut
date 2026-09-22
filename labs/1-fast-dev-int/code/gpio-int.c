// Simple code to measure the time it takes to trigger and
// return from a GPIO interrupt.
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

// Can change these pins to whatever you want.  
// 
// NOTE: some pin numbers let the compiler generate faster 
// code b/c it can load derived constants in fewer instructions
// [useful side quest: write some code to check this claim!]
enum { out_pin = 26, in_pin = 27 };

// counters: only modified by the interrupt handler.
// since they are read by non-interrupt code, we must
// either use memory barriers or mark them as volatile.
static volatile unsigned n_rising_edge, n_falling_edge;
static volatile unsigned n_interrupt;

// interrupt handler: should only be called on gpio 
// transitions from 0->1 or 1->0, nothing else (no timer, 
// etc).  
// all it does:
//  1. increment appropriate counter;
//  2. clear the interrupt;
//  3. return.
void int_vector(uint32_t pc) {
    // in general: we don't know if the non-interrupt code was 
    // using a different bcm device.
    dev_barrier();

    // should only have GPIO interrupts
    if(!gpio_event_detected(in_pin)) 
        panic("unexpected interrupt\n");
    else {
        n_interrupt++;

        // 1->0 (falling) transition.
        if(gpio_read(in_pin) == 0) {
            n_falling_edge++;
            // output("falling edge: pc=%x\n", pc);
        // 0->1 (rising) transition.
        } else {
            n_rising_edge++;
            // output("rising edge: pc=%x\n", pc);
        }
        gpio_event_clear(in_pin);
    }

    // in general: we don't know if the non-interrupt code will 
    // now access a different bcm device so have to do a dev
    // barrier.
    dev_barrier();
}

// driver that triggers and measures the interrupts
// caused by writing to GPIO <pin>.
void test_cost(unsigned pin) { 
    // initial state.
    assert(gpio_read(in_pin) == 0);

    float sum = 0;
    uint32_t c,e;
    for(int i = 0; i < 10; i++) {
        // measure the cost of a rising edge interrupt.
        // by reading cycle counter and spinning until the
        // rising edge count increases (i.e., an interrupt
        // occured).
        c = cycle_cnt_read();

        let r = n_rising_edge;
        gpio_set_on(pin);
        while(n_rising_edge == r)
            ;

        e = cycle_cnt_read();
        output("%d: rising\t= %d cycles\n", i*2, e-c);
        sum += e-c;

        // measure the cost of a falling edge interrupt.
        // by reading cycle counter and spinning until the
        // falling edge count increases (i.e., an interrupt
        // occured).
        c = cycle_cnt_read();
        let f = n_falling_edge;
        gpio_set_off(pin);
        while(n_falling_edge == f)
            ;
        e = cycle_cnt_read();
        output("%d: falling\t= %d cycles\n", i*2+1, e-c);
        sum += e-c;
    }
    output("ave cost = %f\n", sum / 20);
}

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

    // now we are live!
    enable_interrupts();

    //*****************************************************
    // 3. run the test.

    // leave this off initially so its easier to see the effect
    // of speed improvements.
    // caches_enable();

    test_cost(out_pin);
    output("Done!\n");
    return;
}
