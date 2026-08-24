# =============================================================================
# jump_test.s — JAL/JALR Instruction Test
# =============================================================================
# Tests JAL (Jump and Link) and JALR (Jump and Link Register).
#
# Expected final values:
#   x10 = 1   (JAL taken, function returned)
#   x11 = 42  (function return value via JALR)
#   x12 = 2   (nested function calls work)
# =============================================================================

    .section .text
    .global _start

_start:
    addi  x10, x0, 0         # x10 = 0

    # ─────────────────────────────────────────────────────────────
    # TEST 1: JAL — jump to label, link return address in ra
    # ─────────────────────────────────────────────────────────────
    jal   ra, func_add1       # Call func_add1, ra = PC+4
    # After return, x10 should be 1
    # ra should point to here

    # ─────────────────────────────────────────────────────────────
    # TEST 2: JAL to a function that returns a value in x11
    # ─────────────────────────────────────────────────────────────
    jal   ra, func_ret42      # Call func_ret42
    # After return, x11 should be 42

    # ─────────────────────────────────────────────────────────────
    # TEST 3: Nested function call
    # ─────────────────────────────────────────────────────────────
    jal   ra, func_outer      # Call func_outer → calls func_inner
    # After return, x12 should be 2

    # ─────────────────────────────────────────────────────────────
    # All tests passed
    # ─────────────────────────────────────────────────────────────
    ecall

# ═════════════════════════════════════════════════════════════════
# FUNCTIONS
# ═════════════════════════════════════════════════════════════════

# func_add1: x10 += 1, then return
func_add1:
    addi  x10, x10, 1        # x10 = x10 + 1
    jalr  x0, ra, 0          # return (jump to ra, discard link)

# func_ret42: x11 = 42, then return
func_ret42:
    addi  x11, x0, 42        # x11 = 42
    jalr  x0, ra, 0          # return

# func_outer: calls func_inner, which sets x12 = 2
func_outer:
    # Save ra on stack (simplified: use a register since we're in test)
    addi  sp, sp, -4
    sw    ra, 0(sp)           # Save return address

    addi  x12, x0, 1         # x12 = 1
    jal   ra, func_inner     # Call inner function

    lw    ra, 0(sp)           # Restore return address
    addi  sp, sp, 4
    jalr  x0, ra, 0          # return

# func_inner: x12 += 1, then return
func_inner:
    addi  x12, x12, 1        # x12 = x12 + 1 = 2
    jalr  x0, ra, 0          # return
