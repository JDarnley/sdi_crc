__m128i xor_clmul(__m128i a, __m128i b) {
    return _mm_xor_si128(_mm_clmulepi64_si128(a, b, 0x00),
                         _mm_clmulepi64_si128(a, b, 0x11));
}

__m256i broadcast128(const uint16_t* data) {
    __m256i result;
    asm("vbroadcasti128 %1, %0" : "=x"(result) :
                                   "m"(*(const __m128i*)data));
    return result;
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
            __m128i d1 = _mm_loadu_si128((__m128i*)(data + i));
            __m256i d2 = broadcast128(data + i + 8);
            __m256i d3 = broadcast128(data + i + 16);
            __m128i k1 = _mm_setr_epi16(
                16, 16, 64, 64, 1, 1, 4, 4);
            __m256i k23 = _mm256_setr_epi16(
                16,  0, 64,  0, 1, 0, 4, 0,
                 0, 16,  0, 64, 0, 1, 0, 4);
            d1 = _mm_mullo_epi16(k1, d1);
            __m128i k4 = _mm_setr_epi8(
                0,  1, -1,  8,  9, -1, -1, -1,
                2,  3, -1, 10, 11, -1, -1, -1);
            __m128i k5 = _mm_setr_epi8(
                -1, 4,  5, -1, 12, 13, -1, -1,
                -1, 6,  7, -1, 14, 15, -1, -1);
            d1 = _mm_shuffle_epi8(d1, k4) ^ _mm_shuffle_epi8(d1, k5);
            __m128i d1m; // Force a store to memory
            asm("vmovdqa %1, %0" : "=m"(d1m) : "x"(d1) : "memory");
            __m256i cdyd = _mm256_packus_epi32(
                _mm256_madd_epi16(d2, k23),
                _mm256_madd_epi16(d3, k23));
            __m256i k6 = _mm256_setr_epi8(
                -1, -1, -1, -1, -1,  0,  1, -1,
                 4,  5,  8,  9, -1, 12, 13, -1,
                -1, -1, -1, -1, -1,  0,  1, -1,
                 4,  5,  8,  9, -1, 12, 13, -1);
            __m256i k7 = _mm256_setr_epi8(
                -1, -1, -1, -1, -1, -1,  2,  3,
                -1,  6,  7, 10, 11, -1, 14, 15,
                -1, -1, -1, -1, -1, -1,  2,  3,
                -1,  6,  7, 10, 11, -1, 14, 15);
            cdyd = _mm256_shuffle_epi8(cdyd, k6)
                 ^ _mm256_shuffle_epi8(cdyd, k7);
            __m256i m; // Force a store to memory
            asm("vmovdqa %1, %0" : "=m"(m) : "x"(cdyd) : "memory");
            __m128i cd = _mm256_castsi256_si128(cdyd)
                       ^ _mm_loadu_si64(&d1m);
            __m128i yd = _mm_loadu_si128(1 + (__m128i*)&m)
                       ^ _mm_loadu_si64(8 + (char*)&d1m);
            c = _mm_xor_si128(c, cd);
            y = _mm_xor_si128(y, yd);
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
