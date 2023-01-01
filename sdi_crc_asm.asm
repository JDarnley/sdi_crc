
%include "x86util.asm"

SECTION_RODATA 64

sdi_crc_init: dd 0x9f380000, 0x0, 0x0, 0x0
sdi_crc_k1_k2: dq 0x46840000, 0x95450000
sdi_crc_k3_k4: dq 0x46840000, 0x95450000
sdi_crc_k5_k6: dd 0x00000000, 0x14980000, 0x00000000, 0x40000 ;  x^14+64-1 mod P, x^14-1 mod P
sdi_crc_mu dd 0x80040000, 0x5e405011, 0x14980559, 0x4c9bb5d5
sdi_crc_p  dq 0x46001, 0

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

    ;mova m0, [sdi_crc_init]
    ;movd m1, [crcq]
    ;pclmulqdq m1, m0, 0x00
    pxor m1, m1

    mova m0, [sdi_crc_k3_k4]
.fold_by_1_loop:
    movu m2, [dataq]

    REDUCE128 m1, m2, m0, m5

    add dataq, mmsize
    sub lenq, mmsize ; FIXME clean this up
    jg .fold_by_1_loop

.reduction:
    ; acc *= x^14, ~80 bits afterwards
    mova m0, [sdi_crc_k5_k6]
    pclmulqdq m2, m1, m0, 0x00
    pclmulqdq m1, m0, 0x11
    pxor m1, m2

    ; divP = (acc * (x^128 div P)) mod x^128
    mova m0, [sdi_crc_mu]
    pclmulqdq m2, m1, m0, 0x01
    pclmulqdq m5, m1, m0, 0x10
    pxor m2, m5
    psrldq m2, 8
    pclmulqdq m1, m0, 0x11
    pxor m1, m2

    ; modP = (divP * P) div x^128
    pclmulqdq m1, [sdi_crc_p], 0x00
    pextrd [crcq], m1, 0
RET

%endmacro

INIT_XMM sse4
sdi_crc

INIT_XMM avx
sdi_crc