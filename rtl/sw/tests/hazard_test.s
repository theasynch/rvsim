# =============================================================================
# hazard_test.s — Pipeline Hazard Test
# =============================================================================
# Tests that the pipeline correctly handles data hazards through
# forwarding and stalling.
#
# Hazard Scenarios:
#   1. EX→EX forwarding:  ADD x1 → immediate use in ADD x2
#   2. MEM→EX forwarding: ADD x3 → use 2 instructions later
#   3. Load-use stall:    LW x5 → immediate use (requires 1-cycle stall)
#   4. Double hazard:     Both EX and MEM results needed
#   5. Store after load:  LW then SW of same register
#
# Expected final values:
#   x10 = 55   (result of correct forwarding chain)
#   x11 = 100  (result of load + forwarded use)
#   x12 = 42   (double-hazard resolution)
# =============================================================================

    .section .text
    .global _start

_start:
    # ─────────────────────────────────────────────────────────────
    # TEST 1: EX→EX Forwarding (back-to-back register dependency)
    # ─────────────────────────────────────────────────────────────
    addi  x1, x0, 10         # x1 = 10
    add   x2, x1, x1         # x2 = x1 + x1 = 20 (needs EX→EX fwd of x1)
    add   x3, x2, x1         # x3 = x2 + x1 = 30 (needs EX→EX fwd of x2)

    # ─────────────────────────────────────────────────────────────
    # TEST 2: MEM→EX Forwarding (2-cycle gap)
    # ─────────────────────────────────────────────────────────────
    addi  x4, x0, 25         # x4 = 25
    nop                       # 1 cycle gap
    add   x5, x4, x3         # x5 = x4 + x3 = 25 + 30 = 55 (MEM→EX fwd)
    add   x10, x5, x0        # x10 = 55 (final result 1)

    # ─────────────────────────────────────────────────────────────
    # TEST 3: Load-Use Hazard (must stall 1 cycle)
    # ─────────────────────────────────────────────────────────────
    # First, store a value to memory
    lui   x6, 0x10            # x6 = 0x00010000 (data memory base)
    addi  x7, x0, 100        # x7 = 100
    sw    x7, 0(x6)           # MEM[0x10000] = 100

    # Now load it and immediately use it (load-use hazard!)
    lw    x8, 0(x6)           # x8 = MEM[0x10000] = 100 (LOAD in EX)
    add   x11, x8, x0        # x11 = x8 + 0 = 100 (USE in ID → must stall!)

    # ─────────────────────────────────────────────────────────────
    # TEST 4: Double Hazard (both EX and MEM produce needed values)
    # ─────────────────────────────────────────────────────────────
    addi  x20, x0, 20        # x20 = 20
    addi  x21, x0, 22        # x21 = 22 (EX→EX fwd for x20 NOT needed here)
    add   x12, x21, x20      # x12 = 22 + 20 = 42
                              # x21 from EX/MEM (1 cycle old)
                              # x20 from MEM/WB (2 cycles old)

    # ─────────────────────────────────────────────────────────────
    # TEST 5: Store-after-Load (forwarding store data)
    # ─────────────────────────────────────────────────────────────
    lw    x25, 0(x6)          # x25 = MEM[0x10000] = 100
    sw    x25, 4(x6)          # MEM[0x10004] = x25 = 100 (needs fwd)
    lw    x26, 4(x6)          # x26 = MEM[0x10004] = 100 (verify store)

    # ─────────────────────────────────────────────────────────────
    # Done
    # ─────────────────────────────────────────────────────────────
    ecall
