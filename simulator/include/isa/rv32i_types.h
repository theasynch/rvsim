// =============================================================================
// rv32i_types.h — RISC-V 32-bit Base Integer ISA (RV32I) Type Definitions
// =============================================================================
//
// This file defines every type, enum, and constant needed to represent
// RISC-V instructions. Understanding this file IS understanding the ISA.
//
// RISC-V has six instruction formats. Every 32-bit instruction belongs to
// exactly one of them. The format determines where the bits are:
//
//  R-type: [funct7|rs2|rs1|funct3|rd|opcode]  — register-register ops
//  I-type: [    imm[11:0]|rs1|funct3|rd|opcode] — immediate / loads / JALR
//  S-type: [imm[11:5]|rs2|rs1|funct3|imm[4:0]|opcode] — stores
//  B-type: branch variant of S-type (split immediate, PC-relative)
//  U-type: [      imm[31:12]     |rd|opcode]   — LUI, AUIPC
//  J-type: [    imm[20:1]    |rd|opcode]        — JAL
//
// =============================================================================
#pragma once

#include <cstdint>
#include <string>

namespace rv32i {

// ---------------------------------------------------------------------------
// Opcodes (bits [6:0] of every instruction)
// ---------------------------------------------------------------------------
enum class Opcode : uint8_t {
    LOAD    = 0b000'0011,   // LB, LH, LW, LBU, LHU
    STORE   = 0b010'0011,   // SB, SH, SW
    BRANCH  = 0b110'0011,   // BEQ, BNE, BLT, BGE, BLTU, BGEU
    JALR    = 0b110'0111,   // JALR
    JAL     = 0b110'1111,   // JAL
    OP_IMM  = 0b001'0011,   // ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI
    OP      = 0b011'0011,   // ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
    LUI     = 0b011'0111,   // LUI
    AUIPC   = 0b001'0111,   // AUIPC
    SYSTEM  = 0b111'0011,   // ECALL, EBREAK
    FENCE   = 0b000'1111,   // FENCE (treated as NOP in our sim)
    INVALID = 0xFF,
};

// ---------------------------------------------------------------------------
// Instruction format (tells the decoder how to extract fields)
// ---------------------------------------------------------------------------
enum class InstrFormat {
    R, I, S, B, U, J, UNKNOWN
};

// ---------------------------------------------------------------------------
// ALU Operation — what computation the execute stage performs
// ---------------------------------------------------------------------------
enum class ALUOp : uint8_t {
    ADD,    // ADD, ADDI, loads, stores, AUIPC
    SUB,    // SUB, branch comparison (BEQ/BNE via SUB, BLT/BGE via SLT)
    SLL,    // SLL, SLLI
    SLT,    // SLT, SLTI  (signed less-than)
    SLTU,   // SLTU, SLTIU (unsigned less-than)
    XOR,    // XOR, XORI
    SRL,    // SRL, SRLI
    SRA,    // SRA, SRAI
    OR,     // OR, ORI
    AND,    // AND, ANDI
    PASS_A, // AUIPC (pass PC through to adder — PC + imm)
    PASS_B, // LUI (pass immediate, don't add register)
    NOP,    // No operation
};

// ---------------------------------------------------------------------------
// Memory access size (used by load/store)
// ---------------------------------------------------------------------------
enum class MemSize : uint8_t {
    BYTE      = 1,   // LB / SB
    HALFWORD  = 2,   // LH / SH
    WORD      = 4,   // LW / SW
};

// ---------------------------------------------------------------------------
// Control signals — the combinational logic outputs of the Control Unit.
//
// In hardware, these are single bits driving MUXes, enable pins, etc.
// In our simulator, they're a struct that travels through pipeline registers.
// ---------------------------------------------------------------------------
struct ControlSignals {
    bool reg_write   = false;  // Write result to register file at WB stage
    bool mem_read    = false;  // Read from data memory at MEM stage
    bool mem_write   = false;  // Write to data memory at MEM stage
    bool mem_to_reg  = false;  // WB mux: 1=use mem data, 0=use ALU result
    bool alu_src     = false;  // EX mux: 1=immediate operand B, 0=register rs2
    bool branch      = false;  // This instruction may branch (B-type)
    bool jump        = false;  // Unconditional jump (JAL)
    bool jalr        = false;  // Indirect jump (JALR, rd = PC+4)
    bool lui         = false;  // LUI: ALU just passes immediate
    bool auipc       = false;  // AUIPC: ALU adds PC + immediate
    bool is_ecall    = false;  // Simulator halt signal
    bool mem_signed  = true;   // Signed extend memory data (LB vs LBU)
    MemSize mem_size = MemSize::WORD;
    ALUOp   alu_op   = ALUOp::NOP;
};

// ---------------------------------------------------------------------------
// Decoded instruction — what the ID stage produces
// ---------------------------------------------------------------------------
struct DecodedInstr {
    uint32_t raw;           // Raw 32-bit instruction word
    Opcode   opcode;        // Extracted opcode field
    InstrFormat format;     // Which format this instruction uses

    uint8_t  rd;            // Destination register index [0..31]
    uint8_t  rs1;           // Source register 1 index
    uint8_t  rs2;           // Source register 2 index
    uint8_t  funct3;        // Sub-opcode field [14:12]
    uint8_t  funct7;        // Sub-opcode field [31:25]

    int32_t  imm;           // Sign-extended immediate (all formats unified here)

    ControlSignals ctrl;    // Control signals for this instruction
    std::string    mnemonic; // Human-readable, e.g. "ADDI x10, x0, 42"

    bool valid = false;     // False if this is a bubble/NOP
};

// ---------------------------------------------------------------------------
// Pipeline register structures
//
// These are the "flip-flops" between pipeline stages. Each one holds the
// state produced by the PREVIOUS stage and consumed by the NEXT stage.
// A "bubble" (NOP) propagates by zeroing valid and ctrl.reg_write etc.
// ---------------------------------------------------------------------------

struct IFIDReg {
    uint32_t    instruction = 0; // Raw instruction bits fetched from memory
    uint32_t    pc          = 0; // PC of this instruction
    uint32_t    pc_plus4    = 0; // PC + 4 (used for JAL/JALR return address)
    bool        valid       = false; // false = bubble
    std::string asm_str;           // For visualization
};

struct IDEXReg {
    uint32_t    pc          = 0;
    uint32_t    pc_plus4    = 0;
    int32_t     rs1_data    = 0; // Value read from register file for rs1
    int32_t     rs2_data    = 0; // Value read from register file for rs2
    int32_t     imm         = 0; // Sign-extended immediate
    uint8_t     rs1         = 0; // Register INDEX (needed for forwarding checks)
    uint8_t     rs2         = 0; // Register INDEX (needed for forwarding checks)
    uint8_t     rd          = 0; // Destination register index
    ControlSignals ctrl;
    bool        valid       = false;
    std::string asm_str;
};

struct EXMEMReg {
    uint32_t    pc          = 0;
    uint32_t    pc_plus4    = 0;
    int32_t     alu_result  = 0; // Output of ALU (also used as memory address)
    int32_t     rs2_data    = 0; // Data to store (for SW/SH/SB)
    uint8_t     rd          = 0;
    bool        branch_taken= false;
    uint32_t    branch_target= 0; // PC of branch/jump destination
    ControlSignals ctrl;
    bool        valid       = false;
    std::string asm_str;
    // Filled by MEM stage for visualization
    bool        cache_hit   = false;
    bool        is_mem_op   = false;
};

struct MEMWBReg {
    uint32_t    pc          = 0;
    int32_t     alu_result  = 0;
    int32_t     mem_data    = 0; // Data read from memory (for LW/LH/LB)
    uint8_t     rd          = 0;
    ControlSignals ctrl;
    bool        valid       = false;
    std::string asm_str;
};

// ---------------------------------------------------------------------------
// Forwarding MUX select signals
// ---------------------------------------------------------------------------
enum class ForwardSel : uint8_t {
    NONE,       // Use ID/EX pipeline register value (register file output)
    EX_MEM,     // Forward from EX/MEM pipeline register (1-cycle-old ALU result)
    MEM_WB,     // Forward from MEM/WB pipeline register (2-cycle-old result)
};

// ---------------------------------------------------------------------------
// Instruction name — for human-readable output
// ---------------------------------------------------------------------------
std::string opcode_name(Opcode op);

} // namespace rv32i
