__m128i pmovzxwq(const uint16_t* data) {
    __m128i out;
    asm("vpmovzxwq %1, %0" : "=x"(out) :
                              "m"(*(const uint32_t*)data));
    return out;
}

__m128i broadcastss(const uint16_t* data) {
    __m128i result;
    asm("vbroadcastss %1, %0" : "=x"(result) :
                                 "m"(*(const uint32_t*)data));
    return result;
}

__m128i xor3(__m128i a, __m128i b, __m128i c) {
    return _mm_ternarylogic_epi64(a, b, c, 0x96);
}

__m128i pack60x2_4(const uint16_t* data) {
    __m128i shift4  = _mm_setr_epi32(1u <<  4, 0, 1u << ( 4+16), 0);
    __m128i shift14 = _mm_setr_epi32(1u << 14, 0, 1u << (14+16), 0);
    __m128i shift34 = _mm_setr_epi32(0, 1u <<  2, 0, 1u << ( 2+16));
    __m128i shift44 = _mm_setr_epi32(0, 1u << 12, 0, 1u << (12+16));
    __m128i result, hi;
    result = xor3(_mm_madd_epi16(broadcastss(data     ), shift4),
                  _mm_madd_epi16(broadcastss(data +  2), shift14),
                  _mm_madd_epi16(broadcastss(data +  6), shift34));
    hi     =      _mm_madd_epi16(broadcastss(data +  4), shift4)
           ^      _mm_madd_epi16(broadcastss(data + 10), shift34);
    result = xor3(result,
                  _mm_madd_epi16(broadcastss(data +  8), shift44),
                  _mm_slli_epi64(hi, 20));
    return result;
}

__m128i pack60x2_0(const uint16_t* data) {
    __m128i shift10 = _mm_setr_epi32(1u << 10, 0, 1u << (10+16), 0);
    __m128i shift40 = _mm_setr_epi32(0, 1u << 8, 0, 1u << (8+16));
    __m128i result, hi;
    hi     =                        pmovzxwq(data +  4)
           ^      _mm_madd_epi16(broadcastss(data +  6), shift10);
    result = xor3(                  pmovzxwq(data     ),
                  _mm_madd_epi16(broadcastss(data +  2), shift10),
                  _mm_slli_epi64(hi, 20));
    result = xor3(result,
                  _mm_madd_epi16(broadcastss(data +  8), shift40),
                  _mm_slli_epi64(   pmovzxwq(data + 10), 50));
    return result;
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
        // *= x^120 semi-mod P
        // +=
        __m128i lo = pack60x2_4(data + i);
        __m128i hi = pack60x2_0(data + i + 12);
        __m128i k = _mm_setr_epi32(
            0, 0x4b334000 /* x^120+64-1 mod P */,
            0, 0x96d30000 /* x^120-1    mod P */);
        c = xor3(_mm_clmulepi64_si128(c, k, 0x00),
                 _mm_clmulepi64_si128(c, k, 0x11),
                 _mm_unpacklo_epi64(lo, hi));
        y = xor3(_mm_clmulepi64_si128(y, k, 0x00),
                 _mm_clmulepi64_si128(y, k, 0x11),
                 _mm_unpackhi_epi64(lo, hi));
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
