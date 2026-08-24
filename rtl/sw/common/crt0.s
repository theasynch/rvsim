# =============================================================================
# crt0.s — Startup Code for Bare-Metal RISC-V Programs
# =============================================================================
# This is the entry point for all C programs running on our processor.
# It sets up the stack pointer, zeroes BSS, calls main(), then halts.
# =============================================================================

    .section .text.init
    .global _start
    .type _start, @function

_start:
    # ─────────────────────────────────────────────────────────────
    # Set up stack pointer to top of data memory
    # Our dmem is 4KB at address 0x00010000, so SP = 0x00011000
    # ─────────────────────────────────────────────────────────────
    lui   sp, %hi(_stack_top)
    addi  sp, sp, %lo(_stack_top)

    # ─────────────────────────────────────────────────────────────
    # Zero out .bss section
    # ─────────────────────────────────────────────────────────────
    la    t0, _bss_start
    la    t1, _bss_end
.bss_loop:
    bge   t0, t1, .bss_done
    sw    zero, 0(t0)
    addi  t0, t0, 4
    j     .bss_loop
.bss_done:

    # ─────────────────────────────────────────────────────────────
    # Initialize global pointer (for relaxed addressing)
    # ─────────────────────────────────────────────────────────────
    .option push
    .option norelax
    la    gp, __global_pointer$
    .option pop

    # ─────────────────────────────────────────────────────────────
    # Call main()
    # a0 (x10) will hold the return value
    # ─────────────────────────────────────────────────────────────
    li    a0, 0          # argc = 0
    li    a1, 0          # argv = NULL
    jal   ra, main

    # ─────────────────────────────────────────────────────────────
    # Halt: store return value and call ECALL
    # The testbench monitors a0 (x10) for the result
    # ─────────────────────────────────────────────────────────────
    # a0 already contains main's return value
    ecall                # Halt processor

    # Infinite loop (should never reach here)
.halt_loop:
    j     .halt_loop

    .size _start, .-_start
