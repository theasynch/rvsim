# =============================================================================
# load_store_test.s — Load/Store Instruction Test
# =============================================================================
# Tests LW, LH, LB, LHU, LBU, SW, SH, SB with proper sign extension.
#
# We use data memory starting at 0x00010000.
#
# Expected final register values:
#   x10 = 0xDEADBEEF  (word load)
#   x11 = 0xFFFFBEEF  (halfword load, sign-extended)
#   x12 = 0x000000EF  (byte load unsigned)
#   x13 = 0xFFFFFFEF  (byte load signed, 0xEF sign-extends)
#   x14 = 0x0000BEEF  (halfword load unsigned)
#   x15 = 0x00000042  (byte stored + loaded back)
#   x16 = 0x00001234  (halfword stored + loaded back)
# =============================================================================

    .section .text
    .global _start

_start:
    # ── Base address for data memory ──
    lui   x1, 0x10            # x1 = 0x00010000 (data memory base)

    # ── Store a test pattern: 0xDEADBEEF at address 0x10000 ──
    lui   x2, 0xDEADB        # x2 = 0xDEADB000
    addi  x2, x2, 0xFFFFEEF  # x2 = 0xDEADBEEF (adjusted for sign extension)
    sw    x2, 0(x1)           # MEM[0x10000] = 0xDEADBEEF

    # ── TEST 1: LW (Load Word) ──
    lw    x10, 0(x1)          # x10 = MEM[0x10000] = 0xDEADBEEF

    # ── TEST 2: LH (Load Halfword, signed) ──
    lh    x11, 0(x1)          # x11 = sign_extend(0xBEEF) = 0xFFFFBEEF

    # ── TEST 3: LBU (Load Byte, unsigned) ──
    lbu   x12, 0(x1)          # x12 = zero_extend(0xEF) = 0x000000EF

    # ── TEST 4: LB (Load Byte, signed) ──
    lb    x13, 0(x1)          # x13 = sign_extend(0xEF) = 0xFFFFFFEF

    # ── TEST 5: LHU (Load Halfword, unsigned) ──
    lhu   x14, 0(x1)          # x14 = zero_extend(0xBEEF) = 0x0000BEEF

    # ── TEST 6: SB + LB (Store Byte) ──
    addi  x3, x0, 0x42        # x3 = 0x42
    sb    x3, 16(x1)          # MEM[0x10010] = 0x42 (byte)
    lbu   x15, 16(x1)         # x15 = 0x42

    # ── TEST 7: SH + LH (Store Halfword) ──
    addi  x4, x0, 0x1234      # x4 = 0x1234 (sign-extended from imm)
    sh    x4, 20(x1)          # MEM[0x10014] = 0x1234 (halfword)
    lhu   x16, 20(x1)         # x16 = 0x1234

    # ── Halt ──
    ecall
