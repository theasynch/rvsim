# =============================================================================
# add_test.s — Arithmetic Instruction Test
# =============================================================================
# Tests all R-type and I-type arithmetic/logic instructions.
# After execution, expected register values:
#   x1  = 10        (ADDI)
#   x2  = 20        (ADDI)
#   x3  = 30        (ADD)
#   x4  = -10       (SUB: 10-20)
#   x5  = 0xFF      (ANDI)
#   x6  = 0xFF0F    (ORI)
#   x7  = 0xFF00    (XORI)
#   x8  = 40        (SLL: 10<<2)
#   x9  = 5         (SRL: 20>>2)
#   x10 = 1         (SLT: 10<20)
#   x11 = 0         (SLT: 20<10)
#   x12 = 0x12345   (LUI upper bits)
#   x13 = -8        (SRA: -32>>>2)
#   x14 = 0         (SLTU: 20 < 10)
#   x15 = 1         (SLTU: 10 < 20)
# =============================================================================

    .section .text
    .global _start

_start:
    # ── I-type: immediate operations ──
    addi  x1, x0, 10         # x1 = 10
    addi  x2, x0, 20         # x2 = 20

    # ── R-type: register operations ──
    add   x3, x1, x2         # x3 = 10 + 20 = 30
    sub   x4, x1, x2         # x4 = 10 - 20 = -10

    # ── Bitwise I-type ──
    addi  x5, x0, 0xFF       # x5 = 0xFF (temp)
    andi  x5, x5, 0xFF       # x5 = 0xFF & 0xFF = 0xFF
    lui   x6, 0              # x6 = 0
    addi  x6, x0, 0x0F       # x6 = 0x0F
    ori   x6, x6, 0xFF00     # x6 = 0x0F | 0xFF00 = ... (sign extended!)

    addi  x7, x0, 0xFF       # x7 = 0xFF
    xori  x7, x7, 0xFF       # x7 = 0xFF ^ 0xFF = 0

    # ── Shift operations ──
    slli  x8, x1, 2          # x8 = 10 << 2 = 40
    srli  x9, x2, 2          # x9 = 20 >> 2 = 5

    # ── Set-less-than ──
    slt   x10, x1, x2        # x10 = (10 < 20) = 1
    slt   x11, x2, x1        # x11 = (20 < 10) = 0

    # ── LUI (Load Upper Immediate) ──
    lui   x12, 0x12345        # x12 = 0x12345000

    # ── SRA (Arithmetic Right Shift) ──
    addi  x13, x0, -32       # x13 = -32
    srai  x13, x13, 2        # x13 = -32 >>> 2 = -8

    # ── SLTU (unsigned) ──
    sltu  x14, x2, x1        # x14 = (20 < 10) unsigned = 0
    sltu  x15, x1, x2        # x15 = (10 < 20) unsigned = 1

    # ── R-type bitwise ──
    and   x16, x1, x2        # x16 = 10 & 20 = 0
    or    x17, x1, x2        # x17 = 10 | 20 = 30
    xor   x18, x1, x2        # x18 = 10 ^ 20 = 30
    sll   x19, x1, x2        # x19 = 10 << (20 & 0x1F) = 10 << 20
    srl   x20, x2, x1        # x20 = 20 >> (10 & 0x1F) = 20 >> 10 = 0

    # ── SLTI (immediate) ──
    slti  x21, x1, 20        # x21 = (10 < 20) = 1
    slti  x22, x2, 10        # x22 = (20 < 10) = 0
    sltiu x23, x1, 20        # x23 = (10 < 20) unsigned = 1

    # ── Halt ──
    ecall                     # Stop processor
