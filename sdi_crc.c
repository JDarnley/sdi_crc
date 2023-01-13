#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <inttypes.h>

#define NUM_SAMPLES (1920*2)

void upipe_sdi_crc_sse4(uint8_t *data, uintptr_t len, uint32_t *crc);

static void randomise_buffers(uint16_t *src0, int len)
{
    for (int i = 0; i < len; i++) {
        src0[i] = rand() & 0x3ff;
    }
}

static void upipe_uyvy_to_sdi_sep_10_c(uint8_t *c, uint8_t *y, const uint16_t *uyvy, uintptr_t pixels)
{
    uintptr_t width = pixels * 2;
    for (int j = 0; j < width; j += 8) {
        uint16_t u1 = uyvy[j+0];
        uint16_t y1 = uyvy[j+1];
        uint16_t v1 = uyvy[j+2];
        uint16_t y2 = uyvy[j+3];

        uint16_t u2 = uyvy[j+4];
        uint16_t y3 = uyvy[j+5];
        uint16_t v2 = uyvy[j+6];
        uint16_t y4 = uyvy[j+7];

        *c++ = u1 & 0xff;
        *c++ = ((v1 & 0x3f) << 2) | ((u1 & 0x300) >> 8);
        *c++ = ((u2 & 0x0f) << 4) | ((v1 & 0x3c0) >> 6);
        *c++ = ((v2 & 0x03) << 6) | ((u2 & 0x3f0) >> 4);
        *c++ = (v2 >> 2) & 0xff;

        *y++ = y1 & 0xff;
        *y++ = ((y2 & 0x3f) << 2) | ((y1 & 0x300) >> 8);
        *y++ = ((y3 & 0x0f) << 4) | ((y2 & 0x3c0) >> 6);
        *y++ = ((y4 & 0x03) << 6) | ((y3 & 0x3f0) >> 4);
        *y++ = (y4 >> 2) & 0xff;
    }
}

static void upipe_uyvy_to_sdi_sep_60_c(uint64_t *c64, uint64_t *y64, const uint16_t *uyvy, uintptr_t pixels)
{
    uintptr_t width = pixels * 2;
    int extra = 0;
    for (int j = 0; j < width; j += 12) {
        uint64_t c1 = uyvy[j+0];
        uint64_t y1 = uyvy[j+1];
        uint64_t c2 = uyvy[j+2];
        uint64_t y2 = uyvy[j+3];

        uint64_t c3 = uyvy[j+4];
        uint64_t y3 = uyvy[j+5];
        uint64_t c4 = uyvy[j+6];
        uint64_t y4 = uyvy[j+7];

        uint64_t c5 = uyvy[j+8];
        uint64_t y5 = uyvy[j+9];
        uint64_t c6 = uyvy[j+10];
        uint64_t y6 = uyvy[j+11];

        *c64 = (c6 << 50) | (c5 << 40) | (c4 << 30) | (c3 << 20) | (c2 << 10) | (c1 << 0);
        *y64 = (y6 << 50) | (y5 << 40) | (y4 << 30) | (y3 << 20) | (y2 << 10) | (y1 << 0);

        extra ^= 4;
        *c64 <<= extra;
        *y64 <<= extra;

        c64++;
        y64++;
    }
}

#define L(value, shift) ((__uint128_t)(value) << shift)

static void upipe_uyvy_to_sdi_sep_120_c(__uint128_t *c128, __uint128_t *y128, const uint16_t *uyvy, uintptr_t pixels)
{
    uintptr_t width = pixels * 2;
    int extra = 0;
    for (int j = 0; j < width; j += 24) {
        __uint128_t c = L(uyvy[j+0],   0) | L(uyvy[j+2],   10) | L(uyvy[j+4],   20)
                      | L(uyvy[j+6],  30) | L(uyvy[j+8],   40) | L(uyvy[j+10],  50)
                      | L(uyvy[j+12], 60) | L(uyvy[j+14],  70) | L(uyvy[j+16],  80)
                      | L(uyvy[j+18], 90) | L(uyvy[j+20], 100) | L(uyvy[j+22], 110);
        __uint128_t y = L(uyvy[j+1],   0) | L(uyvy[j+3],   10) | L(uyvy[j+5],   20)
                      | L(uyvy[j+7],  30) | L(uyvy[j+9],   40) | L(uyvy[j+11],  50)
                      | L(uyvy[j+13], 60) | L(uyvy[j+15],  70) | L(uyvy[j+17],  80)
                      | L(uyvy[j+19], 90) | L(uyvy[j+21], 100) | L(uyvy[j+23], 110);

        //extra ^= 8;
        *c128++ = c << extra;
        *y128++ = y << extra;
    }
}


static uint32_t crc_sdi_unpacked(uint32_t crc, const uint16_t *data, size_t data_len)
{
    for(int i = 0; i < data_len; i++) {
        crc ^= data[2*i] & 0x3ff;
        for (int k = 0; k < 10; k++)
            crc = crc & 1 ? (crc >> 1) ^ 0x23000 : crc >> 1;
    }

    return crc;
}

static uint32_t crc_update_packed(uint32_t crc, const uint8_t *data, size_t data_len)
{
    for(int i = 0; i < data_len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ 0x23000 : crc >> 1;
    }

    return crc;
}

static uint32_t crc_update_packed60(uint32_t crc, const uint64_t *data, size_t data_len)
{
    uint64_t crc64 = crc;
    for(int i = 0; i < data_len; i++) {
        uint64_t w = data[i];
        if (!(i & 1))
            w >>= 4;
        crc64 ^= w;
        for (int k = 0; k < 60; k++)
            crc64 = crc64 & 1 ? (crc64 >> 1) ^ 0x23000 : crc64 >> 1;
    }

    return (uint32_t)(crc64 & 0xffffffff);
}

int main(int argc, char **argv)
{
    uint16_t src0[NUM_SAMPLES];
    uint8_t c0_packed[NUM_SAMPLES*5/8], y0_packed[NUM_SAMPLES*5/8];
    int packed60_len = (NUM_SAMPLES / 2) * 10 / 60;
    uint64_t c0_packed60[packed60_len], y0_packed60[packed60_len];
    uint32_t crc_c_ref = 0, crc_y_ref = 0,
             crc_c_packed = 0, crc_y_packed = 0,
             crc_c_packed_asm = 0, crc_y_packed_asm = 0,
             crc_c_packed60 = 0, crc_y_packed60 = 0;

    const int packed60_len = (NUM_SAMPLES / 2) * 10 / 60;
    __uint128_t c0_packed120[packed60_len], y0_packed120[packed60_len];

    srand(time(NULL));

    randomise_buffers(src0, NUM_SAMPLES);
    upipe_uyvy_to_sdi_sep_10_c(c0_packed, y0_packed, src0, NUM_SAMPLES/2);
    upipe_uyvy_to_sdi_sep_60_c(c0_packed60, y0_packed60, src0, NUM_SAMPLES/2);

    crc_c_ref = crc_sdi_unpacked(crc_c_ref, src0, NUM_SAMPLES/2);
    crc_y_ref = crc_sdi_unpacked(crc_y_ref, src0+1, NUM_SAMPLES/2);

    crc_c_packed = crc_update_packed(crc_c_packed, c0_packed, NUM_SAMPLES*5/8);
    crc_y_packed = crc_update_packed(crc_y_packed, y0_packed, NUM_SAMPLES*5/8);

    crc_c_packed60 = crc_update_packed60(crc_c_packed60, (uint64_t*)c0_packed60, packed60_len);
    crc_y_packed60 = crc_update_packed60(crc_y_packed60, (uint64_t*)y0_packed60, packed60_len);

    upipe_sdi_crc_sse4((uint8_t*)c0_packed60, packed60_len*8, &crc_c_packed_asm);
    upipe_sdi_crc_sse4((uint8_t*)y0_packed60, packed60_len*8, &crc_y_packed_asm);

    if(crc_c_packed != crc_c_ref)
    {
        printf("crc_c_packed does not match crc_c_ref \n");
        return -1;
    }

    if(crc_y_packed != crc_y_ref)
    {
        printf("crc_y_packed does not match crc_y_ref \n");
        return -1;
    }

    if(crc_c_packed_asm != crc_c_ref)
    {
        printf("crc_c_packed_asm does not match crc_c_ref \n");
        return -1;
    }

    if(crc_y_packed_asm != crc_y_ref)
    {
        printf("crc_y_packed_asm does not match crc_y_ref \n");
        return -1;
    }

    if(crc_c_packed60 != crc_c_ref)
    {
        printf("crc_c_packed60 does not match crc_c_ref \n");
        return -1;
    }

    if(crc_y_packed60 != crc_y_ref)
    {
        printf("crc_c_packed60 does not match crc_y_ref \n");
        return -1;
    }

    printf("Tests pass! \n");

    return 0;
}
