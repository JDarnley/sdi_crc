
%include "x86util.asm"

SECTION_RODATA 64

sdi_crc_k1_k2: dq 0xbd64, 0x7d80
sdi_crc_k3_k4: dq 0x8410, 0x526
sdi_crc_k5_k0: dq 0x5790, 0x000000000
sdi_crc_p_mu:  dq 0x46001, 0x14046001

; second 0xffffffff seems to do nothing, chromium uses it still
sdi_crc_reduction_mask: dd 0xffffffff, 0x0, 0xffffffff, 0x0

SECTION .text

; a, b, constant, tmp
%macro REDUCE128 4
    pclmulqdq %4, %1, %3, 0x00
    pclmulqdq %1, %3, 0x11
    pxor %1, %2
    pxor %1, %4
%endmacro

%macro sdi_crc 0

; upipe_sdi_crc(uint8_t *data, uintptr_t len, uint32_t *crc)
cglobal sdi_crc, 3, 3, 9, data, len, crc
    ; use same register name as chromium
    mova m0, [sdi_crc_k1_k2]

    movu m1, [dataq+mmsize*0]
    movu m2, [dataq+mmsize*1]
    movu m3, [dataq+mmsize*2]
    movu m4, [dataq+mmsize*3]

    add dataq, (mmsize*4)
    sub lenq, (mmsize*4)

    ; TODO: 8-way unroll

.fold_by_4_loop:
    pclmulqdq m5, m1, m0, 0x00
    pclmulqdq m6, m2, m0, 0x00
    pclmulqdq m7, m3, m0, 0x00
    pclmulqdq m8, m4, m0, 0x00

    pclmulqdq m1, m0, 0x11
    pclmulqdq m2, m0, 0x11
    pclmulqdq m3, m0, 0x11
    pclmulqdq m4, m0, 0x11

    pxor m1, m5
    pxor m2, m6
    pxor m3, m7
    pxor m4, m8

    pxor m1, [dataq+mmsize*0]
    pxor m2, [dataq+mmsize*1]
    pxor m3, [dataq+mmsize*2]
    pxor m4, [dataq+mmsize*3]

    add dataq, (mmsize*4)
    sub lenq, (mmsize*4)

    cmp lenq, (mmsize*4)
    jge .fold_by_4_loop

    ; Fold into 128-bits
    mova m0, [sdi_crc_k3_k4]

    REDUCE128 m1, m2, m0, m5
    REDUCE128 m1, m3, m0, m5
    REDUCE128 m1, m4, m0, m5

    test lenq, lenq
    jle .reduction

.fold_by_1_loop:
    movu m2, [dataq]

    REDUCE128 m1, m2, m0, m5

    add dataq, mmsize
    sub lenq, mmsize ; FIXME clean this up
    jg .fold_by_1_loop

.reduction:
    ; 128-bit to 64-bit reduction
    mova m3, [sdi_crc_reduction_mask]

    ; x = (x[0:63] • K4) ^ x[64:127] // 96 bit result
    psrldq m2, m1, 8
    pclmulqdq m1, m0, 0x10
    pxor m1, m2

    mova m0, [sdi_crc_k5_k0]

    ; x = ((x[0:31] as u64) • K5) ^ x[32:95] // 64 bit result
    psrldq m2, m1, 4
    pand m1, m3
    pclmulqdq m1, m0, 0x00
    pxor m2, m1

    ; barrett folding
    mova m0, [sdi_crc_p_mu]

    ; R(x)

    ; T1(x) = ⌊(R(x) % x^32)⌋ • μ
    pand m1, m2, m3
    pclmulqdq m1, m0, 0x10

    ; T2(x) = ⌊(T1(x) % x^32)⌋ • P(x)
    pand m1, m3
    pclmulqdq m1, m0, 0x00

    ; C(x) = R(x) ^ T2(x) / x^32
    pxor m2, m1
    pextrd [crcq], m2, 1
RET

%endmacro

INIT_XMM sse4
sdi_crc

INIT_XMM avx
sdi_crc