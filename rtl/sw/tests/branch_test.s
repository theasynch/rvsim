# =============================================================================
# branch_test.s — Branch Instruction Test
# =============================================================================
# Tests all 6 branch conditions: BEQ, BNE, BLT, BGE, BLTU, BGEU
# Each branch is tested for both taken and not-taken cases.
#
# Expected final register values:
#   x10 = 6  (incremented once per successful test, 6 branch types tested)
#   x11 = 1  (BEQ taken flag)
#   x12 = 1  (BNE taken flag)
#   x13 = 1  (BLT taken flag)
#   x14 = 1  (BGE taken flag)
#   x15 = 1  (BLTU taken flag)
#   x16 = 1  (BGEU taken flag)
# =============================================================================

    .section .text
    .global _start

_start:
    addi  x10, x0, 0         # x10 = test counter (should reach 6)
    addi  x1,  x0, 10        # x1 = 10
    addi  x2,  x0, 20        # x2 = 20
    addi  x3,  x0, 10        # x3 = 10 (same as x1)
    addi  x4,  x0, -5        # x4 = -5 (0xFFFFFFFB)

    # ─────────────────────────────────────────────────────────────
    # TEST 1: BEQ (Branch if Equal)
    # ─────────────────────────────────────────────────────────────
    beq   x1, x3, beq_taken  # x1 == x3 (10 == 10) → should branch
    j     fail                # Should not reach here
beq_taken:
    addi  x10, x10, 1        # test counter++
    addi  x11, x0, 1         # BEQ flag = 1

    beq   x1, x2, fail       # x1 != x2 (10 != 20) → should NOT branch
    # Fall through = correct

    # ─────────────────────────────────────────────────────────────
    # TEST 2: BNE (Branch if Not Equal)
    # ─────────────────────────────────────────────────────────────
    bne   x1, x2, bne_taken  # x1 != x2 → should branch
    j     fail
bne_taken:
    addi  x10, x10, 1
    addi  x12, x0, 1         # BNE flag = 1

    bne   x1, x3, fail       # x1 == x3 → should NOT branch

    # ─────────────────────────────────────────────────────────────
    # TEST 3: BLT (Branch if Less Than, signed)
    # ─────────────────────────────────────────────────────────────
    blt   x1, x2, blt_taken  # 10 < 20 → should branch
    j     fail
blt_taken:
    addi  x10, x10, 1
    addi  x13, x0, 1         # BLT flag = 1

    blt   x2, x1, fail       # 20 < 10 → should NOT branch

    # ─────────────────────────────────────────────────────────────
    # TEST 4: BGE (Branch if Greater or Equal, signed)
    # ─────────────────────────────────────────────────────────────
    bge   x2, x1, bge_taken  # 20 >= 10 → should branch
    j     fail
bge_taken:
    addi  x10, x10, 1
    addi  x14, x0, 1         # BGE flag = 1

    bge   x1, x2, fail       # 10 >= 20 → should NOT branch

    # ─────────────────────────────────────────────────────────────
    # TEST 5: BLTU (Branch if Less Than, unsigned)
    # ─────────────────────────────────────────────────────────────
    bltu  x1, x2, bltu_taken # 10 < 20 (unsigned) → should branch
    j     fail
bltu_taken:
    addi  x10, x10, 1
    addi  x15, x0, 1         # BLTU flag = 1

    # x4 = -5 = 0xFFFFFFFB, which is very large unsigned
    bltu  x4, x1, fail       # 0xFFFFFFFB < 10 unsigned? NO → should NOT branch

    # ─────────────────────────────────────────────────────────────
    # TEST 6: BGEU (Branch if Greater or Equal, unsigned)
    # ─────────────────────────────────────────────────────────────
    bgeu  x2, x1, bgeu_taken # 20 >= 10 (unsigned) → should branch
    j     fail
bgeu_taken:
    addi  x10, x10, 1
    addi  x16, x0, 1         # BGEU flag = 1

    # ─────────────────────────────────────────────────────────────
    # SUCCESS: all 6 tests passed
    # ─────────────────────────────────────────────────────────────
    addi  x10, x10, 0        # x10 should be 6
    ecall                     # Halt — SUCCESS

    # ─────────────────────────────────────────────────────────────
    # FAILURE path
    # ─────────────────────────────────────────────────────────────
fail:
    addi  x10, x0, -1        # x10 = -1 indicates failure
    ecall                     # Halt — FAIL
