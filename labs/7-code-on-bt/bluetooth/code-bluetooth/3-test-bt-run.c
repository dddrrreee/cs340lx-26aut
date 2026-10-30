/* Test the driver's 3n+7 check: without Launch_RAM the answer must stay wrong.
 * The answer slot starts as ~(3n+7), so only the radio running chip.S can
 * make it 3n+7. With n = 0x55555553, 3n+7 = 0 (mod 2^32): an answer slot
 * that started at 0 would pass without running anything. */
#include "rpi.h"
#include "pl011.h"
#include "bt.h"

static const uint8_t chip[] = {
#include "chip_image.h"
};

void notmain(void) {
    pl011_up(115200, CR_RTS);
    bt_reset(10);
    bt_connect(500 * 1000);

    unsigned n = 0x55555553, box[2] = { n, ~(3 * n + 7) }, answer;
    bt_write_ram(LOAD, chip, sizeof chip);
    bt_write_ram(RESULT_AREA, box, sizeof box);
    bt_read_ram(RESULT_AREA + 4, &answer, 4);
    if (answer != ~(3 * n + 7))
        panic("without Launch_RAM the answer changed to %x", answer);
    trace("without Launch_RAM the answer stays ~(3n+7) [answer=%x]\n", answer);

    bt_launch_ram(LOAD | 1);
    bt_read_ram(RESULT_AREA + 4, &answer, 4);
    if (answer != 3 * n + 7)
        panic("after Launch_RAM the answer is %x, not 3n+7", answer);
    trace("after Launch_RAM it is 3n+7 [answer=%x]\n", answer);
}
