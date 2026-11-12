#ifndef SE050_H
#define SE050_H
#include <stdint.h>

// util
uint16_t crc16_x25(const uint8_t *data, uint32_t length);

// link layer: T=1 over I2C
int t1_send(uint8_t pcb, uint8_t len, uint8_t *buf);
int t1_recv(uint8_t *pcb, uint8_t *buf, uint8_t max_len);

// ISO7816-4: TLV
uint8_t *tlv_encode(uint8_t *tlv, uint8_t tag, const uint8_t *val, uint8_t len);
uint8_t *tlv_decode(uint8_t *tlv, uint8_t tlv_len, uint8_t *tag, uint8_t **val, uint16_t *val_len);

// APDU: Application Protocol Data Unit
int apdu_assemble(uint8_t *apdu_frame, uint8_t ins, uint8_t p1, uint8_t p2, uint8_t *data, uint8_t data_len, uint32_t need_resp);
int rapdu_decode(uint8_t *rapdu_frame, int len, uint8_t **body, uint16_t *status);

// SE050 specific functions
int se050_AppletSelect();
int se050_WriteSymmKey(uint8_t *keyid, uint8_t *key, uint8_t key_len);
int se050_MACOneShot_HMAC_SHA1(uint8_t *keyid, uint8_t *msg, uint8_t msg_len, uint8_t *mac);
int se050_DeleteSecureObject(uint8_t *keyid);

// Application layer function
int totp_generate(uint8_t *keyid, int64_t unix_time);

#endif