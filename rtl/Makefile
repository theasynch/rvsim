# =============================================================================
# Makefile — Verilator Build Automation for RV32I Processor
# =============================================================================
# Targets:
#   make sim          — Build + run full processor simulation
#   make test_alu     — Build + run ALU unit test
#   make lint         — Lint all RTL source files
#   make wave         — Open GTKWave with latest trace
#   make clean        — Remove all generated files
#
# Variables:
#   PROGRAM=path/to/program.hex   — Hex file to load into instruction memory
#   MAX_CYCLES=10000              — Maximum simulation cycles
# =============================================================================

# Verilator binary
VERILATOR ?= verilator

# Source directories
RTL_DIR   = src
TB_DIR    = tb
WAVE_DIR  = waves
OBJ_DIR   = obj_dir

# All RTL source files
RTL_PKG   = $(RTL_DIR)/pkg/rv32i_pkg.sv
RTL_SRC   = $(RTL_DIR)/datapath/alu.sv \
            $(RTL_DIR)/datapath/register_file.sv \
            $(RTL_DIR)/datapath/imm_gen.sv \
            $(RTL_DIR)/datapath/branch_comp.sv \
            $(RTL_DIR)/memory/imem.sv \
            $(RTL_DIR)/memory/dmem.sv \
            $(RTL_DIR)/core/control_unit.sv \
            $(RTL_DIR)/hazard/forwarding_unit.sv \
            $(RTL_DIR)/hazard/hazard_unit.sv \
            $(RTL_DIR)/core/rv32i_top.sv

# Default program to load
PROGRAM     ?= sw/tests/add_test.hex
MAX_CYCLES  ?= 10000

# Verilator flags
VFLAGS = --sv --cc --exe --build \
         --trace \
         -Wall \
         -Wno-UNUSED \
         -Wno-UNDRIVEN \
         --top-module rv32i_top \
         -I$(RTL_DIR)/pkg \
         -GIMIT_INIT_FILE=\"$(PROGRAM)\"

VFLAGS_ALU = --sv --cc --exe --build \
             --top-module alu \
             -I$(RTL_DIR)/pkg \
             -Wall

# =============================================================================
# TARGETS
# =============================================================================

.PHONY: all sim test_alu lint wave clean dirs

all: sim

# Create output directories
dirs:
	@mkdir -p $(WAVE_DIR)
	@mkdir -p $(OBJ_DIR)

# ─────────────────────────────────────────────────────────────────────────────
# Full processor simulation
# ─────────────────────────────────────────────────────────────────────────────
sim: dirs
	$(VERILATOR) $(VFLAGS) \
		$(RTL_PKG) $(RTL_SRC) \
		$(TB_DIR)/sim_main.cpp \
		-o Vrv32i_top
	@echo ""
	@echo "════════════════════════════════════════════════════════"
	@echo "  Running RV32I Processor Simulation"
	@echo "  Program: $(PROGRAM)"
	@echo "════════════════════════════════════════════════════════"
	@echo ""
	./$(OBJ_DIR)/Vrv32i_top +max_cycles=$(MAX_CYCLES)

# ─────────────────────────────────────────────────────────────────────────────
# ALU unit test
# ─────────────────────────────────────────────────────────────────────────────
test_alu: dirs
	$(VERILATOR) $(VFLAGS_ALU) \
		$(RTL_PKG) $(RTL_DIR)/datapath/alu.sv \
		$(TB_DIR)/tb_alu.cpp \
		-o Valu
	./$(OBJ_DIR)/Valu

# ─────────────────────────────────────────────────────────────────────────────
# Run individual assembly test programs
# ─────────────────────────────────────────────────────────────────────────────
test_add: PROGRAM = sw/tests/add_test.hex
test_add: sim

test_branch: PROGRAM = sw/tests/branch_test.hex
test_branch: sim

test_load_store: PROGRAM = sw/tests/load_store_test.hex
test_load_store: sim

test_hazard: PROGRAM = sw/tests/hazard_test.hex
test_hazard: sim

test_jump: PROGRAM = sw/tests/jump_test.hex
test_jump: sim

# ─────────────────────────────────────────────────────────────────────────────
# Run C programs
# ─────────────────────────────────────────────────────────────────────────────
test_fibonacci: PROGRAM = sw/programs/fibonacci.hex
test_fibonacci: sim

# ─────────────────────────────────────────────────────────────────────────────
# Lint all RTL
# ─────────────────────────────────────────────────────────────────────────────
lint:
	$(VERILATOR) --lint-only --sv -Wall \
		-I$(RTL_DIR)/pkg \
		$(RTL_PKG) $(RTL_SRC)
	@echo ""
	@echo "✓ All RTL files passed lint checks"

# ─────────────────────────────────────────────────────────────────────────────
# Open waveform viewer
# ─────────────────────────────────────────────────────────────────────────────
wave:
	gtkwave $(WAVE_DIR)/rv32i_trace.vcd &

# ─────────────────────────────────────────────────────────────────────────────
# Clean
# ─────────────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(OBJ_DIR)
	rm -f $(WAVE_DIR)/*.vcd
	@echo "✓ Cleaned"
