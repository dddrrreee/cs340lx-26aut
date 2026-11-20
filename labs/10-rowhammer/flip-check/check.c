#include "rpi.h"

void rowhammer_selftest(void);

void notmain(void) {
    rowhammer_selftest();
}
