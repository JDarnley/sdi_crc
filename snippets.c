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
    polynomial c = (crcs[0] * x-14) semi-mod P;
    polynomial y = (crcs[1] * x-14) semi-mod P;
    for (size_t i = 0; i < n; i += 24) {
        c = (c * x120) semi-mod P;
        y = (y * x120) semi-mod P;
        c = c + pack120(data + i);
        y = y + pack120(data + i + 1);
    }
    c = (c * x14) semi-mod P;
    y = (y * x14) semi-mod P;
    crcs[0] = c mod P;
    crcs[1] = y mod P;
}

polynomial pack120(const polynomial* data) {
    return pack60(data) * x64 + pack60(data + 12) * x4;
}

polynomial pack60(const polynomial* data) {
    polynomial result = 0;
    for (size_t i = 0; i < 12; i += 2) {
        result = result * x10 + data[i];
    }
    return result;
}
