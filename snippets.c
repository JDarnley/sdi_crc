void crc_sdi(uint32_t* crcs, const uint16_t* data, size_t n) {
    uint32_t c = crcs[0];
    uint32_t y = crcs[1];
    for (size_t i = 0; i < n; i += 2) {
        c ^= data[i];
        y ^= data[i+1];
        for (int k = 0; k < 10; k++) {
            c = c & 1 ? (c >> 1) ^ 0x23000 : c >> 1;
            y = y & 1 ? (y >> 1) ^ 0x23000 : y >> 1;
        }
    }
    crcs[0] = c;
    crcs[1] = y;
}
