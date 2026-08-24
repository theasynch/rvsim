// =============================================================================
// tb_alu.cpp — ALU Unit Test (Verilator)
// =============================================================================
// Standalone testbench for the ALU module. Tests all 11 operations with
// multiple operand values and checks results.
// =============================================================================

#include <verilated.h>
#include "Valu.h"

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <tuple>
#include <string>

// ALU operation codes (must match rv32i_pkg.sv)
enum AluOp {
    ALU_ADD    = 0b0000,
    ALU_SUB    = 0b0001,
    ALU_SLL    = 0b0010,
    ALU_SLT    = 0b0011,
    ALU_SLTU   = 0b0100,
    ALU_XOR    = 0b0101,
    ALU_SRL    = 0b0110,
    ALU_SRA    = 0b0111,
    ALU_OR     = 0b1000,
    ALU_AND    = 0b1001,
    ALU_PASS_B = 0b1010,
};

#define GRN "\033[32m"
#define RED "\033[31m"
#define RST "\033[0m"
#define BLD "\033[1m"

struct TestCase {
    std::string name;
    uint32_t a, b;
    AluOp op;
    uint32_t expected;
};

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Valu* dut = new Valu;

    std::vector<TestCase> tests = {
        // ADD
        {"ADD: 5 + 3",         5,          3,          ALU_ADD,  8},
        {"ADD: 0 + 0",         0,          0,          ALU_ADD,  0},
        {"ADD: -1 + 1",        0xFFFFFFFF, 1,          ALU_ADD,  0},
        {"ADD: overflow",      0x7FFFFFFF, 1,          ALU_ADD,  0x80000000},

        // SUB
        {"SUB: 10 - 3",        10,         3,          ALU_SUB,  7},
        {"SUB: 0 - 1",         0,          1,          ALU_SUB,  0xFFFFFFFF},
        {"SUB: 5 - 5",         5,          5,          ALU_SUB,  0},

        // SLL
        {"SLL: 1 << 4",        1,          4,          ALU_SLL,  16},
        {"SLL: 0xFF << 8",     0xFF,       8,          ALU_SLL,  0xFF00},
        {"SLL: shift by 0",    42,         0,          ALU_SLL,  42},

        // SLT (signed)
        {"SLT: 3 < 5",         3,          5,          ALU_SLT,  1},
        {"SLT: 5 < 3",         5,          3,          ALU_SLT,  0},
        {"SLT: -1 < 1",        0xFFFFFFFF, 1,          ALU_SLT,  1},
        {"SLT: 1 < -1",        1,          0xFFFFFFFF, ALU_SLT,  0},

        // SLTU (unsigned)
        {"SLTU: 3 < 5",        3,          5,          ALU_SLTU, 1},
        {"SLTU: 0xFFFFFFFF<1", 0xFFFFFFFF, 1,          ALU_SLTU, 0},
        {"SLTU: 1<0xFFFFFFFF", 1,          0xFFFFFFFF, ALU_SLTU, 1},

        // XOR
        {"XOR: 0xFF ^ 0x0F",   0xFF,       0x0F,       ALU_XOR,  0xF0},
        {"XOR: self",          0xDEADBEEF, 0xDEADBEEF, ALU_XOR,  0},

        // SRL (logical right shift)
        {"SRL: 0x80 >> 4",     0x80,       4,          ALU_SRL,  0x08},
        {"SRL: -1 >> 16",      0xFFFFFFFF, 16,         ALU_SRL,  0x0000FFFF},

        // SRA (arithmetic right shift)
        {"SRA: -8 >> 2",       0xFFFFFFF8, 2,          ALU_SRA,  0xFFFFFFFE},
        {"SRA: 8 >> 2",        8,          2,          ALU_SRA,  2},

        // OR
        {"OR: 0xF0 | 0x0F",    0xF0,       0x0F,       ALU_OR,   0xFF},

        // AND
        {"AND: 0xFF & 0x0F",   0xFF,       0x0F,       ALU_AND,  0x0F},
        {"AND: all ones",      0xFFFFFFFF, 0xFFFFFFFF, ALU_AND,  0xFFFFFFFF},

        // PASS_B
        {"PASS_B: 0xDEAD",    0,          0xDEADBEEF, ALU_PASS_B, 0xDEADBEEF},
    };

    int pass = 0, fail = 0;

    printf(BLD "\n╔═══════════════════════════════════════════════╗\n" RST);
    printf(BLD "║        ALU Unit Test — %zu test cases           ║\n" RST, tests.size());
    printf(BLD "╚═══════════════════════════════════════════════╝\n\n" RST);

    for (auto& t : tests) {
        dut->a  = t.a;
        dut->b  = t.b;
        dut->op = t.op;
        dut->eval();

        bool ok = (dut->result == t.expected);

        if (ok) {
            printf(GRN "  [PASS] " RST "%-30s  a=0x%08X b=0x%08X → 0x%08X\n",
                   t.name.c_str(), t.a, t.b, dut->result);
            pass++;
        } else {
            printf(RED "  [FAIL] " RST "%-30s  a=0x%08X b=0x%08X → 0x%08X (expected 0x%08X)\n",
                   t.name.c_str(), t.a, t.b, dut->result, t.expected);
            fail++;
        }
    }

    // Zero flag test
    dut->a = 5; dut->b = 5; dut->op = ALU_SUB; dut->eval();
    if (dut->zero == 1) {
        printf(GRN "  [PASS] " RST "Zero flag: 5-5 → zero=1\n");
        pass++;
    } else {
        printf(RED "  [FAIL] " RST "Zero flag: 5-5 → zero=%d (expected 1)\n", dut->zero);
        fail++;
    }

    dut->a = 5; dut->b = 3; dut->op = ALU_SUB; dut->eval();
    if (dut->zero == 0) {
        printf(GRN "  [PASS] " RST "Zero flag: 5-3 → zero=0\n");
        pass++;
    } else {
        printf(RED "  [FAIL] " RST "Zero flag: 5-3 → zero=%d (expected 0)\n", dut->zero);
        fail++;
    }

    printf("\n" BLD "Results: " GRN "%d passed" RST ", " RED "%d failed" RST " out of %d\n\n",
           pass, fail, pass + fail);

    delete dut;
    return fail > 0 ? 1 : 0;
}
