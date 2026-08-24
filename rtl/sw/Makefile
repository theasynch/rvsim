# =============================================================================
# sw/Makefile — Cross-Compilation for Bare-Metal RISC-V Programs
# =============================================================================
# Uses riscv32-unknown-elf-gcc to compile C and assembly programs
# into hex files loadable by the Verilator testbench.
#
# Targets:
#   make all          — Build all test programs and C programs
#   make tests        — Build assembly test programs only
#   make programs     — Build C programs only
#   make clean        — Remove generated files
# =============================================================================

# RISC-V toolchain prefix
CROSS   ?= riscv32-unknown-elf-
CC       = $(CROSS)gcc
AS       = $(CROSS)as
LD       = $(CROSS)ld
OBJCOPY  = $(CROSS)objcopy
OBJDUMP  = $(CROSS)objdump

# Compiler flags for bare-metal RV32I
CFLAGS   = -march=rv32i -mabi=ilp32 \
           -ffreestanding -nostdlib -nostartfiles \
           -O1 -Wall \
           -T common/linker.ld

ASFLAGS  = -march=rv32i -mabi=ilp32

# Directories
TEST_DIR = tests
PROG_DIR = programs
COMMON   = common
BUILD    = build

# Source files
ASM_TESTS = $(wildcard $(TEST_DIR)/*.s)
C_PROGS   = $(wildcard $(PROG_DIR)/*.c)

# Output files
ASM_HEX   = $(patsubst $(TEST_DIR)/%.s, $(TEST_DIR)/%.hex, $(ASM_TESTS))
C_HEX     = $(patsubst $(PROG_DIR)/%.c, $(PROG_DIR)/%.hex, $(C_PROGS))

# =============================================================================
.PHONY: all tests programs clean

all: tests programs

tests: $(ASM_HEX)
	@echo "✓ All assembly tests compiled"

programs: $(C_HEX)
	@echo "✓ All C programs compiled"

# ─────────────────────────────────────────────────────────────────────────────
# Assembly test programs (.s → .hex)
# These are self-contained (no crt0 needed)
# ─────────────────────────────────────────────────────────────────────────────
$(TEST_DIR)/%.hex: $(TEST_DIR)/%.s
	@mkdir -p $(BUILD)
	$(AS) $(ASFLAGS) -o $(BUILD)/$*.o $<
	$(LD) -Ttext=0x0 -o $(BUILD)/$*.elf $(BUILD)/$*.o
	$(OBJCOPY) -O binary $(BUILD)/$*.elf $(BUILD)/$*.bin
	python3 bin2hex.py $(BUILD)/$*.bin $@
	$(OBJDUMP) -d $(BUILD)/$*.elf > $(BUILD)/$*.dump
	@echo "  → $@"

# ─────────────────────────────────────────────────────────────────────────────
# C programs (.c → .hex)
# Linked with crt0.s startup code
# ─────────────────────────────────────────────────────────────────────────────
$(PROG_DIR)/%.hex: $(PROG_DIR)/%.c $(COMMON)/crt0.s $(COMMON)/linker.ld
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/$*.elf $(COMMON)/crt0.s $<
	$(OBJCOPY) -O binary -j .text $(BUILD)/$*.elf $(BUILD)/$*.text.bin
	python3 bin2hex.py $(BUILD)/$*.text.bin $@
	$(OBJDUMP) -d $(BUILD)/$*.elf > $(BUILD)/$*.dump
	@echo "  → $@"

clean:
	rm -rf $(BUILD)
	rm -f $(TEST_DIR)/*.hex $(PROG_DIR)/*.hex
	@echo "✓ Cleaned"
