#ifndef I2C_H
#define I2C_H
#include <stdint.h>
// init i2c
void i2c_init();
// returns >= 0 on success, < 0 on error (NAK/timeout)
int i2c_xfer(int is_read, uint8_t *buf, uint32_t len);
// returns >= 0 on success, < 0 on error (NAK/timeout), retried up to 1000 times
int i2c_xfer_poll(int is_read, uint8_t *buf, uint32_t len);

#endif