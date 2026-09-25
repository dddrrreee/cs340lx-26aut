/*
 * This is our timer interrupt code changed to see if wfi
 * disables the cycle counter.
 */
#include "rpi.h"
#include "cycle-count.h"

// defined in <interrupts-asm.S>
void disable_interrupts(void);
void enable_interrupts(void);

//*******************************************************
// interrupt initialization and support code.

// timer interrupt number defined in 
//     bcm 113: periph interrupts table.
// used in:
//   - Basic pending (p113);
//   - Basic interrupt enable (p 117) 
//   - Basic interrupt disable (p118)
#define ARM_Timer_IRQ   (1<<0)

// registers for ARM interrupt control
// bcm2835, p112   [starts at 0x2000b200]


enum {
    IRQ_Base            = 0x2000b200,
    IRQ_basic_pending   = IRQ_Base+0x00,    // 0x200
    IRQ_pending_1       = IRQ_Base+0x04,    // 0x204
    IRQ_pending_2       = IRQ_Base+0x08,    // 0x208
    IRQ_FIQ_control     = IRQ_Base+0x0c,    // 0x20c
    IRQ_Enable_1        = IRQ_Base+0x10,    // 0x210
    IRQ_Enable_2        = IRQ_Base+0x14,    // 0x214
    IRQ_Enable_Basic    = IRQ_Base+0x18,    // 0x218
    IRQ_Disable_1       = IRQ_Base+0x1c,    // 0x21c
    IRQ_Disable_2       = IRQ_Base+0x20,    // 0x220
    IRQ_Disable_Basic   = IRQ_Base+0x24,    // 0x224
};

// registers for ARM timer
// bcm 14.2 p 196
enum { 
    ARM_Timer_Base      = 0x2000B400,
    ARM_Timer_Load      = ARM_Timer_Base + 0x00, // p196
    ARM_Timer_Value     = ARM_Timer_Base + 0x04, // read-only
    ARM_Timer_Control   = ARM_Timer_Base + 0x08,

    ARM_Timer_IRQ_Clear = ARM_Timer_Base + 0x0c,

    // Errata fo p198: 
    // neither are register 0x40C raw is 0x410, masked is 0x414
    ARM_Timer_IRQ_Raw   = ARM_Timer_Base + 0x10,
    ARM_Timer_IRQ_Masked   = ARM_Timer_Base + 0x14,

    ARM_Timer_Reload    = ARM_Timer_Base + 0x18,
    ARM_Timer_PreDiv    = ARM_Timer_Base + 0x1c,
    ARM_Timer_Counter   = ARM_Timer_Base + 0x20,
};

//**************************************************
// one-time initialization of general purpose 
// interrupt state.
static void interrupt_init(void) {
    printk("about to install interrupt handlers\n");

    // turn off global interrupts.  (should be off
    // already for today)
    disable_interrupts();

    // put interrupt flags in known state by disabling
    // all interrupt sources (1 = disable).
    //  BCM2835 manual, section 7.5 , 112
    PUT32(IRQ_Disable_1, 0xffffffff);
    PUT32(IRQ_Disable_2, 0xffffffff);
    dev_barrier();

    // Copy interrupt vector table and FIQ handler.
    //   - <_interrupt_table>: start address 
    //   - <_interrupt_table_end>: end address 
    // these are defined as labels in <interrupt-asm.S>
    extern uint32_t _interrupt_table[];
    extern uint32_t _interrupt_table_end[];

    // A2-16: first interrupt code address at <0> (reset)
    uint32_t *dst = (void *)0;

    // copy the handlers to <dst>
    uint32_t *src = _interrupt_table;
    unsigned n = _interrupt_table_end - src;

    // these writes better not migrate!
    gcc_mb();
    for(int i = 0; i < n; i++)
        dst[i] = src[i];
    gcc_mb();
}

// initialize timer interrupts.
// <prescale> can be 1, 16, 256. see the timer value.
// NOTE: a better interface = specify the timer period.
// worth doing as an extension!
static 
void timer_init(uint32_t prescale, uint32_t ncycles) {
    //**************************************************
    // now that we are sure the global interrupt state is
    // in a known, off state, setup and enable 
    // timer interrupts.
    printk("setting up timer interrupts\n");

    // assume we don't know what was happening before.
    dev_barrier();
    
    // bcm p 116
    // write a 1 to enable the timer inerrupt , 
    // "all other bits are unaffected"
    PUT32(IRQ_Enable_Basic, ARM_Timer_IRQ);

    // dev barrier b/c the ARM timer is a different device
    // than the interrupt controller.
    dev_barrier();

    // Timer frequency = Clk/256 * Load 
    //   - so smaller <Load> = = more frequent.
    PUT32(ARM_Timer_Load, ncycles);


    // bcm p 197
    enum { 
        // note errata!  not a 23 bit.
        ARM_TIMER_CTRL_32BIT        = ( 1 << 1 ),
        ARM_TIMER_CTRL_PRESCALE_1   = ( 0 << 2 ),
        ARM_TIMER_CTRL_PRESCALE_16  = ( 1 << 2 ),
        ARM_TIMER_CTRL_PRESCALE_256 = ( 2 << 2 ),
        ARM_TIMER_CTRL_INT_ENABLE   = ( 1 << 5 ),
        ARM_TIMER_CTRL_ENABLE       = ( 1 << 7 ),
    };

    uint32_t v = 0;
    switch(prescale) {
    case 1: v = ARM_TIMER_CTRL_PRESCALE_1; break;
    case 16: v = ARM_TIMER_CTRL_PRESCALE_16; break;
    case 256: v = ARM_TIMER_CTRL_PRESCALE_256; break;
    default: panic("illegal prescale=%d\n", prescale);
    }

    // Q: if you change prescale?
    PUT32(ARM_Timer_Control,
            ARM_TIMER_CTRL_32BIT |
            ARM_TIMER_CTRL_ENABLE |
            ARM_TIMER_CTRL_INT_ENABLE |
            v);

    // done modifying timer: do a dev barrier since
    // we don't know what device gets used next.
    dev_barrier();
}

// catch if unexpected exceptions occur.
// these are referenced in <interrupt-asm.S>
// none of them should be called for our example.
void fast_interrupt_vector(unsigned pc) {
    panic("unexpected fast interrupt: pc=%x\n", pc);
}
void syscall_vector(unsigned pc) {
    panic("unexpected syscall: pc=%x\n", pc);
}
void reset_vector(unsigned pc) {
    panic("unexpected reset: pc=%x\n", pc);
}
void undefined_instruction_vector(unsigned pc) {
    panic("unexpected undef-inst: pc=%x\n", pc);
}
void prefetch_abort_vector(unsigned pc) {
    panic("unexpected prefetch abort: pc=%x\n", pc);
}
void data_abort_vector(unsigned pc) {
    panic("unexpected data abort: pc=%x\n", pc);
}

//*******************************************************
// timer interrupt handler: this should get called.

// Q: if you make not volatile?
static volatile unsigned cnt, period, period_sum;

// called by <interrupt-asm.S> on each interrupt.
void interrupt_vector(unsigned pc) {
    // we don't know what the client code was doing, so
    // start with a device barrier in case it was in
    // the middle of using a device (slow: you can 
    // do tricks to remove this.)
    dev_barrier();

    // get the interrupt source: typically if you have 
    // one interrupt enabled, you'll have > 1, so have
    // to disambiguate what the source was.
    unsigned pending = GET32(IRQ_basic_pending);

    // if this isn't true, could be a GPU interrupt 
    // (as discussed in Broadcom): just return.  
    // [confusing, since we didn't enable!]
    if((pending & ARM_Timer_IRQ) == 0)
        return;

    // Clear the ARM Timer interrupt: 
    // Q: what happens, exactly, if we delete?
    PUT32(ARM_Timer_IRQ_Clear, 1);

    // note: <staff-src/timer.c:timer_get_usec_raw()> 
    // accesses the timer device, which is different 
    // than the interrupt subsystem.  so we need
    // a dev_barrier() before.
    dev_barrier();

    cnt++;

    dev_barrier();    
}


// count cycles when we use wfi vs busy wait.
void measure_wfi(void) {
    void wfi(void);

    // take one interrupt so measurement is clean.
    wfi();
    let s1 = cycle_cnt_read();
    wfi();
    let e1 = cycle_cnt_read();

    let s2 = cycle_cnt_read();
    let c = cnt;
    while(cnt==c)
        ;
    let e2 = cycle_cnt_read();

    output("   wfi cycles = %d\n", e1-s1);
    output("no wfi cycles = %d\n", e2-s2);
}

// simple driver: initialize and then run. 
void notmain() {
    //**************************************************
    // interrupt setup.

    // setup general interrupt subsystem.
    interrupt_init();

    // now setup timer interrupts.
    //  - Q: if you change 0x100?
    //  - Q: if you change 16?
    timer_init(16, 0x100);

    // it's go time: enable global interrupts and we will 
    // be live.
    printk("gonna enable ints globally!\n");
    // Q: what happens (&why) if you don't do?
    enable_interrupts();
    printk("enabled!\n");

    measure_wfi();
}
