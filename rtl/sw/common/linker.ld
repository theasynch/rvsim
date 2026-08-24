/* ============================================================================
 * linker.ld — Linker Script for Bare-Metal RISC-V Programs
 * ============================================================================
 * Memory layout for our rv32i_top processor:
 *
 *   Instruction Memory (IMEM): 0x00000000 — 0x00000FFF  (4KB, read-only)
 *   Data Memory (DMEM):        0x00010000 — 0x00010FFF  (4KB, read-write)
 *   Stack:                     grows downward from 0x00011000
 *
 * The IMEM is loaded via $readmemh in Verilog, so the .text section must
 * start at address 0x0. The linker produces a flat binary that maps directly
 * to IMEM words.
 * ============================================================================ */

OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    IMEM (rx)  : ORIGIN = 0x00000000, LENGTH = 4K
    DMEM (rwx) : ORIGIN = 0x00010000, LENGTH = 4K
}

SECTIONS
{
    /* ── Code ── */
    .text :
    {
        *(.text.init)    /* _start must be first */
        *(.text .text.*)
    } > IMEM

    /* ── Read-only data ── */
    .rodata :
    {
        *(.rodata .rodata.*)
    } > DMEM

    /* ── Initialized data ── */
    .data :
    {
        __global_pointer$ = . + 0x800;
        *(.sdata .sdata.*)
        *(.data .data.*)
    } > DMEM

    /* ── BSS (zero-initialized) ── */
    .bss (NOLOAD) :
    {
        _bss_start = .;
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = .;
    } > DMEM

    /* ── Stack ── */
    /* Stack grows downward from top of DMEM */
    _stack_top = ORIGIN(DMEM) + LENGTH(DMEM);

    /* Discard unwanted sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame*)
    }
}
