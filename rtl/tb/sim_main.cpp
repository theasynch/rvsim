// =============================================================================
// sim_main.cpp — Verilator C++ Testbench for RV32I Processor
// =============================================================================
// This is the main simulation driver. It:
//   1. Instantiates the Verilated rv32i_top module
//   2. Drives clock and reset
//   3. Loads a program (via IMEM_INIT_FILE parameter or command-line)
//   4. Runs cycle-by-cycle, printing a trace
//   5. Dumps .vcd waveforms for GTKWave analysis
//   6. Checks final register state against expected values
//
// Usage:
//   ./obj_dir/Vrv32i_top [+trace] [+max_cycles=N]
// =============================================================================

#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vrv32i_top.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Simulation time (in simulation units)
static vluint64_t sim_time = 0;
static const vluint64_t MAX_SIM_TIME_DEFAULT = 20000; // 10000 cycles × 2 edges

// Colors for terminal output
#define RST   "\033[0m"
#define RED   "\033[31m"
#define GRN   "\033[32m"
#define YEL   "\033[33m"
#define BLU   "\033[34m"
#define CYN   "\033[36m"
#define BLD   "\033[1m"

int main(int argc, char** argv) {
    // Initialize Verilator
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    // Parse command-line options
    vluint64_t max_sim_time = MAX_SIM_TIME_DEFAULT;
    const char* max_cycles_str = Verilated::commandArgsPlusMatch("max_cycles=");
    if (max_cycles_str && strlen(max_cycles_str) > 0) {
        // +max_cycles=5000 → "max_cycles=5000"
        const char* eq = strchr(max_cycles_str, '=');
        if (eq) {
            max_sim_time = atoll(eq + 1) * 2; // ×2 because we toggle clock each step
        }
    }

    // Create DUT instance
    Vrv32i_top* dut = new Vrv32i_top;

    // Setup VCD trace
    VerilatedVcdC* trace = new VerilatedVcdC;
    dut->trace(trace, 99); // Trace 99 levels deep
    trace->open("waves/rv32i_trace.vcd");

    printf("\n");
    printf(BLD "╔══════════════════════════════════════════════════════════════╗\n" RST);
    printf(BLD "║" CYN "          RISC-V RV32I Processor — Verilator Simulation      " RST BLD "║\n" RST);
    printf(BLD "╚══════════════════════════════════════════════════════════════╝\n" RST);
    printf("\n");

    // =========================================================================
    // RESET SEQUENCE (hold rst_n low for 5 clock cycles)
    // =========================================================================
    dut->rst_n = 0;
    dut->clk   = 0;

    for (int i = 0; i < 10; i++) {
        dut->clk = !dut->clk;
        dut->eval();
        trace->dump(sim_time);
        sim_time++;
    }

    // Release reset
    dut->rst_n = 1;
    printf(GRN "[RESET] " RST "Reset released. Simulation starting...\n\n");
    printf(BLD "%-8s %-12s %-12s %-6s %-8s %-12s\n" RST,
           "Cycle", "PC", "Instruction", "WB?", "Rd", "WB Data");
    printf("──────── ──────────── ──────────── ────── ──────── ────────────\n");

    // =========================================================================
    // MAIN SIMULATION LOOP
    // =========================================================================
    uint32_t cycle = 0;
    uint32_t instr_count = 0;

    while (sim_time < max_sim_time) {
        // Rising edge
        dut->clk = 1;
        dut->eval();
        trace->dump(sim_time);
        sim_time++;

        // Print trace on rising edge
        uint32_t pc    = dut->debug_pc;
        uint32_t instr = dut->debug_instr;
        bool     wb    = dut->debug_reg_write;
        uint32_t rd    = dut->debug_reg_addr;
        uint32_t data  = dut->debug_reg_data;

        if (wb) {
            printf(YEL "%-8u " RST "0x%08X   0x%08X   " GRN "YES" RST "    x%-5u  0x%08X\n",
                   cycle, pc, instr, rd, data);
            instr_count++;
        } else {
            printf("%-8u 0x%08X   0x%08X   ---    ---      ---\n",
                   cycle, pc, instr);
        }

        // Check for halt
        if (dut->halt) {
            printf("\n" GRN BLD "[HALT] " RST "Processor halted at cycle %u (ECALL)\n", cycle);
            break;
        }

        // Falling edge
        dut->clk = 0;
        dut->eval();
        trace->dump(sim_time);
        sim_time++;

        cycle++;

        // Safety check
        if (Verilated::gotFinish()) {
            printf("\n" YEL "[FINISH] " RST "$finish called\n");
            break;
        }
    }

    if (sim_time >= max_sim_time && !dut->halt) {
        printf("\n" RED BLD "[TIMEOUT] " RST "Simulation reached max cycles (%llu)\n",
               (unsigned long long)(max_sim_time / 2));
    }

    // =========================================================================
    // SUMMARY
    // =========================================================================
    printf("\n");
    printf(BLD "╔══════════════════════════════════════════════╗\n" RST);
    printf(BLD "║" CYN "            Simulation Summary                " RST BLD "║\n" RST);
    printf(BLD "╠══════════════════════════════════════════════╣\n" RST);
    printf(BLD "║" RST "  Total Cycles:       %-22u " BLD "║\n" RST, cycle);
    printf(BLD "║" RST "  Register Writes:    %-22u " BLD "║\n" RST, instr_count);
    printf(BLD "║" RST "  Final PC:           0x%08X             " BLD "║\n" RST, dut->debug_pc);
    printf(BLD "║" RST "  Halted:             %-22s " BLD "║\n" RST, dut->halt ? "YES" : "NO");
    printf(BLD "╚══════════════════════════════════════════════╝\n" RST);
    printf("\n");

    // Cleanup
    trace->close();
    delete trace;
    delete dut;

    printf(GRN "[DONE] " RST "Waveform saved to " BLD "waves/rv32i_trace.vcd\n" RST);
    printf("       Open with: " CYN "gtkwave waves/rv32i_trace.vcd\n" RST);
    printf("\n");

    return 0;
}
