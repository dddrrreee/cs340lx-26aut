#include "rpi.h"
#include "xfer.h"

uint32_t crc32_add(uint32_t crc, const void *data, uint32_t n) {
    const uint8_t *p = data;
    crc = ~crc;
    while (n--) {
        crc ^= *p++;
        for (int k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

static uint32_t get_u32(void) {
    uint32_t u = 0;
    for (int i = 0; i < 4; i++)
        u |= (uint32_t)uart_get8() << (8 * i);
    return u;
}

uint32_t xfer_get(const char *name, void *buf) {
    uart_put8(0x01);
    for (const char *p = "GET "; *p; p++)
        uart_put8(*p);
    for (const char *p = name; *p; p++)
        uart_put8(*p);
    uart_put8('\n');

    // skip anything until "FILE"
    const char *magic = "FILE";
    for (int i = 0; i < 4; ) {
        int c = uart_get8();
        i = (c == magic[i]) ? i + 1 : (c == 'F');
    }
    uint32_t n = get_u32(), crc = get_u32();
    if (n == 0xffffffff)
        panic("the laptop doesn't have <%s>\n", name);

    // no printing in here: the uart only buffers 8 bytes
    uint8_t *p = buf;
    for (uint32_t i = 0; i < n; i++)
        p[i] = uart_get8();
    if (crc32_add(0, buf, n) != crc)
        panic("<%s> got garbled on the way over (bad crc)\n", name);
    return n;
}

static void put_u32(uint32_t u) {
    for (int i = 0; i < 4; i++)
        uart_put8(u >> (8 * i));
}

void xfer_send_start(uint32_t size, uint32_t crc) {
    uart_put8(0x02);
    put_u32(size);
    put_u32(crc);
}

void xfer_send(const void *data, uint32_t n) {
    const uint8_t *p = data;
    while (n--)
        uart_put8(*p++);
}
