
%include "x86util.asm"

SECTION_RODATA 64

sdi_crc_init: dd 0x9f380000, 0x0, 0x0, 0x0
sdi_crc_k1_k2: dq 0x46840000, 0x95450000
sdi_crc_k3_k4: dq 0x46840000, 0x95450000
sdi_crc_k5_k6: dd 0x00000000, 0x14980000, 0x00000000, 0x40000 ;  x^14+64-1 mod P, x^14-1 mod P
sdi_crc_mu dd 0x80040000, 0x5e405011, 0x14980559, 0x4c9bb5d5
sdi_crc_p  dq 0x46001, 0

mult:
    dw 16,  0, 64,  0, 1, 0, 4, 0 ; chroma
    dw  0, 16,  0, 64, 0, 1, 0, 4 ; luma

shuf_lo_even:
times 8 db -1
db 0,1, -1, 4,5, 8,9, -1

shuf_lo_odd:
times 8 db -1
db -1, 2,3, -1, 6,7, 10,11

shuf_hi_even:
db 0,1, 4,5, -1, 8,9, -1
times 8 db -1

shuf_hi_odd:
db -1, 2,3, 6,7, -1, 10,11
times 8 db -1

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

INIT_YMM avx2
; packing stub
cglobal stub, 4, 5, 16, dstc, dsty, src, len, offset
    shl lenq, 2
    xor offsetd, offsetd
    mova m15, [mult]
    VBROADCASTI128 m14, [shuf_lo_even]
    VBROADCASTI128 m13, [shuf_hi_even]
    VBROADCASTI128 m12, [shuf_lo_odd]
    VBROADCASTI128 m11, [shuf_hi_odd]

    .loop:
        VBROADCASTI128 m0, [srcq+offsetq]
        VBROADCASTI128 m1, [srcq+offsetq+16]
        VBROADCASTI128 m2, [srcq+offsetq+32]

        pmaddwd m0, m15 ; chroma | luma
        pmaddwd m1, m15 ; chroma | luma
        pmaddwd m2, m15 ; chroma | luma

        packusdw m0, m1
        packusdw m2, m2
        palignr  m2, m2, m0, 12 ; prepend last 4 bytes of m0 onto m2

        pshufb m3, m0, m14
        pshufb m4, m0, m12
        pshufb m5, m2, m13
        pshufb m6, m2, m11

        por m0, m3, m4
        por m1, m5, m6

        palignr m0, m1, m0, 8 ; data is in center 120 bits

        movu         [dstcq], xm0
        vextracti128 [dstyq], ym0, 1
        add dstcq, 16
        add dstyq, 16
        add offsetq, 48
        cmp offsetq, lenq
    jl .loop
RET

%macro END_1 3 ; accumulator, temp, constant
    pclmulqdq %2, %1, %3, 0x00
    pclmulqdq %1, %1, %3, 0x11
    pxor      %1, %2
%endmacro

%macro END_2 4 ; acc, temp1, temp2, const
    pclmulqdq %2, %1, %4, 0x01
    pclmulqdq %3, %1, %4, 0x10
    pxor      %2, %3
%endmacro

%macro END_3 3 ; acc, temp, const
    psrldq    %2, 8
    pclmulqdq %1, %3, 0x11
    pxor      %1, %2
%endmacro

cglobal calc, 4, 5, 16, crcc, crcy, src, len, offset
    shl lenq, 2
    xor offsetd, offsetd
    mova m15, [mult]
    VBROADCASTI128 m14, [shuf_lo_even]
    VBROADCASTI128 m13, [shuf_hi_even]
    VBROADCASTI128 m12, [shuf_lo_odd]
    VBROADCASTI128 m11, [shuf_hi_odd]

    mova xm10, [sdi_crc_k3_k4]
    pxor xm9, xm9 ; crc "accumulator"
    pxor xm8, xm8

    .loop:
        VBROADCASTI128 m0, [srcq+offsetq]
        VBROADCASTI128 m1, [srcq+offsetq+16]
        VBROADCASTI128 m2, [srcq+offsetq+32]

        pmaddwd m0, m15 ; chroma | luma
        pmaddwd m1, m15 ; chroma | luma
        pmaddwd m2, m15 ; chroma | luma

        packusdw m0, m1
        packusdw m2, m2
        palignr  m2, m2, m0, 12 ; prepend last 4 bytes of m0 onto m2

        pshufb m3, m0, m14
        pshufb m4, m0, m12
        pshufb m5, m2, m13
        pshufb m6, m2, m11

        por m0, m3, m4
        por m1, m5, m6

        palignr m0, m1, m0, 8 ; data is in center 120 bits

        ; movu         [dstcq], xm0
        ; vextracti128 [dstyq], ym0, 1

        vextracti128 xm1, ym0, 1
        REDUCE128 xm9, xm0, xm10, xm2
        REDUCE128 xm8, xm1, xm10, xm3

        add offsetq, 48
        cmp offsetq, lenq
    jl .loop

    mova xm15, [sdi_crc_k5_k6]
    mova xm14, [sdi_crc_mu]
    mova xm13, [sdi_crc_p]

    END_1 xm9, xm0, xm15
    END_1 xm8, xm2, xm15

    END_2 xm9, xm0, xm1, xm14
    END_2 xm8, xm2, xm3, xm14
    END_3 xm9, xm0, xm14
    END_3 xm8, xm2, xm14

    pclmulqdq  xm9, xm13, 0x00
    pclmulqdq  xm8, xm13, 0x00

    movd      [crccq], xm9
    movd      [crcyq], xm8
RET
