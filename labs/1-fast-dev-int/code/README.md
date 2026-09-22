## Make GPIO interrupts fast.

The example code measures how long it takes to handle a GPIO generated
interrupt: 
  - `gpio-int.c`: has the C code to setup GPIO interrupts and measure.
  - `interrupts-asm.S` has the interrupt/exception table and the 
    asembly code to forward interrupts to C code.

The code is simple but slow.  Your job is to speed it up as much as
possible.
