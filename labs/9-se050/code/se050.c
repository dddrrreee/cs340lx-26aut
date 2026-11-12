#include "se050.h"

#define FILL_ME 42

// use this to toggle the N(S) bit in the I-blocks, to test the R-blocks and S-blocks handling.
static int n_s_toggle = 0;
static int n_r_toggle = 0;

// T1 constants
enum
{
    T1_NAD_HD2SE = 0x5A,
    T1_NAD_SE2HD = 0xA5,
    T1_HDR_LEN = 3,
    T1_CRC_LEN = 2,
    T1_LEN_EXTENDED = 0xFF,
    T1_IFSC_DEFAULT = 254,
    T1_FRAME_MAX = T1_HDR_LEN + 2 + 255 + T1_CRC_LEN,

    // I-block: 0 N(S) M 0 0 0 0 0
    T1_I_NS = 1 << 6,
    T1_I_M = 1 << 5,
    // R-block: 1 0 0 N(R) 0 0 err err
    T1_R = 0x80,
    T1_R_NR = 1 << 4,
    T1_R_ERR_CRC = 0x01,
    T1_R_ERR_OTHER = 0x02,
    // S-block: may need to support S-blocks for WTX, resync, etc. but not absolutely needed for this lab.
    T1_S_RESP = 0x20,
    T1_S_RESYNC_REQ = 0xC0,
    T1_S_IFS_REQ = 0xC1,
    T1_S_ABORT_REQ = 0xC2,
    T1_S_WTX_REQ = 0xC3,
    T1_S_END_APDU_REQ = 0xC5,
    T1_S_CHIP_RESET_REQ = 0xC6,
    T1_S_GET_ATR_REQ = 0xC7,
    T1_S_SOFT_RESET_REQ = 0xCF,
};

// SE050 constants
enum
{
    // 4.3.3 instruction
    INS_WRITE = 0x01,
    INS_READ = 0x02,
    INS_CRYPTO = 0x03,
    INS_MGMT = 0x04,
    INS_TRANSIENT = 0x80,
    // 4.3.4 P1
    P1_DEFAULT = 0x00,
    P1_HMAC = 0x05,
    P1_MAC = 0x0D,
    // 4.3.5 P2
    P2_DEFAULT = 0x00,
    P2_DELETE_OBJECT = 0x28,
    P2_GENERATE_ONESHOT = 0x45,
    P2_VALIDATE_ONESHOT = 0x46,
    // 4.3.9 tags
    TAG_1 = 0x41,
    TAG_2 = 0x42,
    TAG_3 = 0x43,
    TAG_4 = 0x44,
    TAG_5 = 0x45,
    // 4.3.18 MACAlgo
    MAC_HMAC_SHA1 = 0x18,

    OBJECT_ID_SIZE = 4,
    HMAC_SHA1_LEN = 20,
};

static uint16_t crc16_x25_step(uint16_t crc, const uint8_t *data, uint32_t length)
{
    // use as is
    for (uint32_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0x8408;
            else
                crc >>= 1;
    }
    return crc;
}

uint16_t crc16_x25(const uint8_t *data, uint32_t length)
{
    // use as is
    return ~crc16_x25_step(0xFFFF, data, length);
}

int t1_send(uint8_t pcb, uint8_t len, uint8_t *buf)
{
    uint8_t frame[len + FILL_ME];
    todo("build frame here!");
    return i2c_xfer_poll(0, frame, FILL_ME);
}

int t1_recv(uint8_t *pcb, uint8_t *buf, uint8_t max_len)
{
    todo("read and decode frame here!");
    uint8_t header[3];
    // read header, decode header
    // write decoded pcb to output
    *pcb = FILL_ME;
    int len = FILL_ME;
    if (len > max_len)
        return -2;
    // read buffer our
    uint8_t crc_bytes[2];
    // read CRC, calculate on buf, and compare
    return len; // success
}

uint8_t *tlv_encode(uint8_t *tlv, uint8_t tag, const uint8_t *val, uint8_t len)
{
    todo("encode TLV here!");
    // encode tlv into tlv buffer, return pointer to next free byte in tlv buffer (so that you can chain multiple TLVs encode together)
    return tlv + FILL_ME;
}

uint8_t *tlv_decode(uint8_t *tlv, uint8_t tlv_len, uint8_t *tag, uint8_t **val, uint16_t *val_len)
{
    todo("decode TLV here!");
    // decode tlv from tlv buffer, return pointer to next TLV in buffer
    // (or NULL if tlv_len is too short for the header or the value)
    return tlv + FILL_ME;
}

int apdu_assemble(uint8_t *apdu_frame, uint8_t ins, uint8_t p1, uint8_t p2, uint8_t *data, uint8_t data_len, uint32_t need_resp)
{
    todo("assemble APDU here!");
    // need_resp can be used for both the flag to indicate if response is needed, and also the Le value if needed.
    // Le = (need_resp >> 8) & 0xff and lower 8 bits is boolean flag to indicate if Le is needed.
    // assemble APDU into output buffer, return length of APDU / error of your choice (>= 0 for success, < 0 for error)
}

int rapdu_decode(uint8_t *rapdu_frame, int len, uint8_t **body, uint16_t *status)
{
    todo("decode RAPDU here!");
    // return <0 to indicate error, >=0 for body length
    // can be simple as moving the body pointer to the start of the body, and calculating the length.
}

int se050_AppletSelect()
{
    todo("select SE050 applet here!");
    // put in SE050 applet AID
    // return >=0 for success, <0 for error
}

int se050_WriteSymmKey(uint8_t *keyid, uint8_t *key, uint8_t key_len)
{
    todo("write symmetric key to SE050 here!");
    // keyid is a four byte array, key is the bin of the key in key_len bytes.
    // return >=0 for success, <0 for error
}

int se050_MACOneShot_HMAC_SHA1(uint8_t *keyid, uint8_t *msg, uint8_t msg_len, uint8_t *mac)
{
    todo("generate MAC using SE050 here!");
    // keyid is a four byte array, msg is the bin of the message in msg_len bytes.
    // mac is the output buffer for the MAC, with fixed length of 20 bytes for HMAC-SHA1.
    // return >=0 for success, <0 for error
}

int se050_DeleteSecureObject(uint8_t *keyid)
{
    todo("delete secure object from SE050 here!");
    // keyid is a four byte array.
    // return >=0 for success, <0 for error
}

int totp_generate(uint8_t *keyid, int64_t unix_time)
{
    todo("generate TOTP using SE050 here!");
    // keyid is a four byte array, time is the current unix time in seconds.
    // return the 6 digit TOTP code, or negative value for error.
}