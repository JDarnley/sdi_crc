uint16_t table[1u << 10];

void make_table() {
    for (uint32_t idx = 0; idx < (1u << 10); idx++) {
        uint32_t c = idx;
        for (int k = 0; k < 10; k++) {
            c = c & 1 ? (c >> 1) ^ 0x23000 : c >> 1;
        }
        table[idx] = (c >> 2);
    }
}

void crc_sdi(uint32_t* crcs, const uint16_t* data, size_t n) {
    uint32_t c = crcs[0];
    uint32_t y = crcs[1];
    for (size_t i = 0; i < n; i += 2) {
        c ^= data[i];
        y ^= data[i+1];
        c = (table[c & 0x3ff] << 2) ^ (c >> 10);
        y = (table[y & 0x3ff] << 2) ^ (y >> 10);
    }
    crcs[0] = c;
    crcs[1] = y;
}

void crc_sdi(polynomial* crcs, const polynomial* data, size_t n) {
    polynomial c = crcs[0];
    polynomial y = crcs[1];
    for (size_t i = 0; i < n; i += 2) {
        c += data[i]   * x8;
        y += data[i+1] * x8;
        for (int k = 0; k < 10; k++) {
            c = (c * x1) mod P; // P is x18 + x5 + x4 + x0
            y = (y * x1) mod P;
        }
    }
    crcs[0] = c;
    crcs[1] = y;
}
