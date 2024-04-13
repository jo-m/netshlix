#include "rfc2435.h"

#include <stdint.h>
#include <string.h>

#define u_char uint8_t
#define u_short uint16_t

/*
 * Table K.1 from JPEG spec.
 */
static const int jpeg_luma_quantizer[64] = {
    16, 11, 10, 16, 24,  40,  51,  61,  12, 12, 14, 19, 26,  58,  60,  55,
    14, 13, 16, 24, 40,  57,  69,  56,  14, 17, 22, 29, 51,  87,  80,  62,
    18, 22, 37, 56, 68,  109, 103, 77,  24, 35, 55, 64, 81,  104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99};

/*
 * Table K.2 from JPEG spec.
 */
static const int jpeg_chroma_quantizer[64] = {
    17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99, 24, 26, 56, 99, 99, 99,
    99, 99, 47, 66, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};

/*
 * Call MakeTables with the Q factor and two u_char[64] return arrays
 */
static void MakeTables(int q, u_char *lqt, u_char *cqt) {
    int i;
    int factor = q;

    if (q < 1) factor = 1;
    if (q > 99) factor = 99;
    if (q < 50)
        q = 5000 / factor;
    else
        q = 200 - factor * 2;
    for (i = 0; i < 64; i++) {
        int lq = (jpeg_luma_quantizer[i] * q + 50) / 100;
        int cq = (jpeg_chroma_quantizer[i] * q + 50) / 100;

        /* Limit the quantizers to 1 <= q <= 255 */
        if (lq < 1)
            lq = 1;
        else if (lq > 255)
            lq = 255;
        lqt[i] = lq;

        if (cq < 1)
            cq = 1;
        else if (cq > 255)
            cq = 255;
        cqt[i] = cq;
    }
}

static const u_char lum_dc_codelens[] = {
    0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
};

static const u_char lum_dc_symbols[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static const u_char lum_ac_codelens[] = {
    0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d,
};

static const u_char lum_ac_symbols[] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61,
    0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52,
    0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25,
    0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45,
    0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64,
    0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83,
    0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
    0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3,
    0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
    0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa,
};

static const u_char chm_dc_codelens[] = {
    0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
};

static const u_char chm_dc_symbols[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static const u_char chm_ac_codelens[] = {
    0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77,
};

static const u_char chm_ac_symbols[] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61,
    0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33,
    0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18,
    0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44,
    0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63,
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
    0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca,
    0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa,
};

static u_char *MakeQuantHeader(u_char *p, const u_char *qt, int tableNo) {
    *p++ = 0xff;
    *p++ = 0xdb; /* DQT */
    *p++ = 0;    /* length msb */
    *p++ = 67;   /* length lsb */
    *p++ = tableNo;
    memcpy(p, qt, 64);
    return (p + 64);
}

static u_char *MakeHuffmanHeader(u_char *p, const u_char *codelens, int ncodes,
                                 const u_char *symbols, int nsymbols, int tableNo, int tableClass) {
    *p++ = 0xff;

    *p++ = 0xc4;                  /* DHT */
    *p++ = 0;                     /* length msb */
    *p++ = 3 + ncodes + nsymbols; /* length lsb */
    *p++ = (tableClass << 4) | tableNo;
    memcpy(p, codelens, ncodes);
    p += ncodes;
    memcpy(p, symbols, nsymbols);
    p += nsymbols;
    return (p);
}

static u_char *MakeDRIHeader(u_char *p, u_short dri) {
    *p++ = 0xff;
    *p++ = 0xdd;       /* DRI */
    *p++ = 0x0;        /* length msb */
    *p++ = 4;          /* length lsb */
    *p++ = dri >> 8;   /* dri msb */
    *p++ = dri & 0xff; /* dri lsb */
    return (p);
}

/*
 *  Arguments:
 *    type, width, height: as supplied in RTP/JPEG header
 *    lqt, cqt: quantization tables as either derived from
 *         the Q field using MakeTables() or as specified
 *         in section 4.2.
 *    dri: restart interval in MCUs, or 0 if no restarts.
 *
 *    p: pointer to return area
 *
 *  Return value:
 *    The length of the generated headers.
 *
 *    Generate a frame and scan headers that can be prepended to the
 *    RTP/JPEG data payload to produce a JPEG compressed image in
 *    interchange format (except for possible trailing garbage and
 *    absence of an EOI marker to terminate the scan).
 */
static int MakeHeaders(u_char *p, int type, int w, int h, const u_char *lqt, const u_char *cqt,
                       u_short dri) {
    u_char *start = p;

    /* convert from blocks to pixels */
    w <<= 3;
    h <<= 3;
    *p++ = 0xff;
    *p++ = 0xd8; /* SOI */

    p = MakeQuantHeader(p, lqt, 0);
    p = MakeQuantHeader(p, cqt, 1);

    if (dri != 0) p = MakeDRIHeader(p, dri);

    *p++ = 0xff;
    *p++ = 0xc0;   /* SOF */
    *p++ = 0;      /* length msb */
    *p++ = 17;     /* length lsb */
    *p++ = 8;      /* 8-bit precision */
    *p++ = h >> 8; /* height msb */
    *p++ = h;      /* height lsb */
    *p++ = w >> 8; /* width msb */
    *p++ = w;      /* wudth lsb */
    *p++ = 3;      /* number of components */
    *p++ = 0;      /* comp 0 */
    if (type == 0)
        *p++ = 0x21; /* hsamp = 2, vsamp = 1 */
    else
        *p++ = 0x22; /* hsamp = 2, vsamp = 2 */
    *p++ = 0;        /* quant table 0 */
    *p++ = 1;        /* comp 1 */
    *p++ = 0x11;     /* hsamp = 1, vsamp = 1 */
    *p++ = 1;        /* quant table 1 */
    *p++ = 2;        /* comp 2 */
    *p++ = 0x11;     /* hsamp = 1, vsamp = 1 */
    *p++ = 1;        /* quant table 1 */
    p = MakeHuffmanHeader(p, lum_dc_codelens, sizeof(lum_dc_codelens), lum_dc_symbols,
                          sizeof(lum_dc_symbols), 0, 0);
    p = MakeHuffmanHeader(p, lum_ac_codelens, sizeof(lum_ac_codelens), lum_ac_symbols,
                          sizeof(lum_ac_symbols), 0, 1);
    p = MakeHuffmanHeader(p, chm_dc_codelens, sizeof(chm_dc_codelens), chm_dc_symbols,
                          sizeof(chm_dc_symbols), 1, 0);
    p = MakeHuffmanHeader(p, chm_ac_codelens, sizeof(chm_ac_codelens), chm_ac_symbols,
                          sizeof(chm_ac_symbols), 1, 1);
    *p++ = 0xff;
    *p++ = 0xda; /* SOS */
    *p++ = 0;    /* length msb */
    *p++ = 12;   /* length lsb */
    *p++ = 3;    /* 3 components */
    *p++ = 0;    /* comp 0 */
    *p++ = 0;    /* huffman table 0 */
    *p++ = 1;    /* comp 1 */
    *p++ = 0x11; /* huffman table 1 */
    *p++ = 2;    /* comp 2 */
    *p++ = 0x11; /* huffman table 1 */
    *p++ = 0;    /* first DCT coeff */
    *p++ = 63;   /* last DCT coeff */
    *p++ = 0;    /* sucessive approx. */

    return (p - start);
}

void rfc2435_make_tables(int q, uint8_t *lqt, uint8_t *cqt) { MakeTables(q, lqt, cqt); }

int rfc2435_make_headers(uint8_t *p, int type, int w, int h, const uint8_t *lqt, const uint8_t *cqt,
                         uint16_t dri) {
    return MakeHeaders(p, type, w, h, lqt, cqt, dri);
}
