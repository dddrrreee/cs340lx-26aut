#include <stdint.h>
#include "i2c.h"
#include "gpio.h"
#include "rpi.h"

// NOTE: this I2C driver roughly the same as the course one, but have to handle error retries

void i2c_init()
{
    // set GPIO alt
    // I used frequency of 100kHz, feel free to adjust within the specs
    // set slave address (given in header as SLAVE)

    // Note: this is regular I2C init, can just use the staff one
}

// returns >= 0 on success, < 0 on error (NAK/timeout)
int i2c_xfer(int is_read, uint8_t *buf, uint32_t len)
{
    // Do the regular i2c read, but give an error code if the transfer fails (NAK or timeout)
    // staff code gives a panic here, which can't be used for this chip since it NAKs a lot.
}

// returns >= 0 on success, < 0 on error (NAK/timeout)
int i2c_xfer_poll(int is_read, uint8_t *buf, uint32_t len)
{
    // wrapper for i2c_xfer with a fix amount of delay and retries (1000) before giving up and returning -1
}