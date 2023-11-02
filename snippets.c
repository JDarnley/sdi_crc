uint64_t pack60(const uint16_t* data) {
    return (((uint64_t)data[ 0]) <<  4) ^
           (((uint64_t)data[ 2]) << 14) ^
           (((uint64_t)data[ 4]) << 24) ^
           (((uint64_t)data[ 6]) << 34) ^
           (((uint64_t)data[ 8]) << 44) ^
           (((uint64_t)data[10]) << 54);
}

__m128i pack120(const uint16_t* data) {
    return _mm_set_epi64x(pack60(data + 12) >> 4, pack60(data));
}

__m128i xor_clmul(__m128i a, __m128i b) {
    return _mm_xor_si128(_mm_clmulepi64_si128(a, b, 0x00),
                         _mm_clmulepi64_si128(a, b, 0x11));
}

void crc_sdi(uint32_t* crcs, const uint16_t* data, size_t n) {
    __m128i c = _mm_cvtsi32_si128(crcs[0]);
    __m128i y = _mm_cvtsi32_si128(crcs[1]);
    { // *= x^-14 semi-mod P
        __m128i k = _mm_cvtsi32_si128(
            0x9f380000 /* x^-14-(64-18)-32-1 mod P */);
        c = _mm_clmulepi64_si128(c, k, 0x00);
        y = _mm_clmulepi64_si128(y, k, 0x00);
    }
    for (size_t i = 0; i < n; i += 24) {
        { // *= x^120 semi-mod P
            __m128i k = _mm_setr_epi32(
                0, 0x4b334000 /* x^120+64-1 mod P */,
                0, 0x96d30000 /* x^120-1    mod P */);
            c = xor_clmul(c, k);
            y = xor_clmul(y, k);
        }
        { // +=
            c = _mm_xor_si128(c, pack120(data + i));
            y = _mm_xor_si128(y, pack120(data + i + 1));
        }
    }
    { // *= x^14 semi-mod P
        __m128i k = _mm_setr_epi32(
            0, 0x14980000 /* x^14+64-1 mod P */,
            0, 0x00040000 /* x^14-1    mod P */);
        c = xor_clmul(c, k);
        y = xor_clmul(y, k);
    }
    { // mod P
        __m128i k = _mm_setr_epi32( /* x^128-1 div P */
            0x14980559, 0x4c9bb5d5,
            0x80040000, 0x5e405011);
        c = _mm_xor_si128(_mm_srli_si128(xor_clmul(c, k), 8),
                          _mm_clmulepi64_si128(c, k, 0x01));
        y = _mm_xor_si128(_mm_srli_si128(xor_clmul(y, k), 8),
                          _mm_clmulepi64_si128(y, k, 0x01));
        __m128i P = _mm_cvtsi32_si128(0x46001 /* P */);
        c = _mm_clmulepi64_si128(c, P, 0x00);
        y = _mm_clmulepi64_si128(y, P, 0x00);
    }
    crcs[0] = _mm_cvtsi128_si32(c);
    crcs[1] = _mm_cvtsi128_si32(y);
}
