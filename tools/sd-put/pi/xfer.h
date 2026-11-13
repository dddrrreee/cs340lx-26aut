#ifndef __XFER_H__
#define __XFER_H__
#include <stdint.h>

// Ask the laptop for a file and receive it into <buf>. Returns its size.
// On the wire:  pi -> laptop   0x01 "GET <name>\n"
//               laptop -> pi   "FILE" <u32 size> <u32 crc32> <bytes>
uint32_t xfer_get(const char *name, void *buf);

// Send a file to the laptop, in pieces: xfer_send_start(), then the bytes
// via xfer_send() (any number of calls).
//     pi -> laptop   0x02 <u32 size> <u32 crc32> <bytes>
void xfer_send_start(uint32_t size, uint32_t crc);
void xfer_send(const void *data, uint32_t n);

// Standard CRC32 (same as Python's zlib.crc32). Chain calls by passing the
// previous result as <crc>; start with 0.
uint32_t crc32_add(uint32_t crc, const void *data, uint32_t n);
#endif
