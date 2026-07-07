// =============================================================================
// decoder.h — RISC-V RV32I Instruction Decoder
// =============================================================================
//
// The decoder is the heart of the ID (Instruction Decode) stage. Given a raw
// 32-bit instruction word, it extracts every field and generates the control
// signals that drive the rest of the pipeline for that instruction.
//
// Hardware equivalent: A combinational logic block (no state) that translates
// the opcode/funct3/funct7 bits into register MUX selects and enable signals.
// =============================================================================
#pragma once

#include "rv32i_types.h"
#include <cstdint>
#include <string>

namespace rv32i {

class Decoder {
public:
    // -----------------------------------------------------------------------
    // decode() — the main entry point.
    //
    // Takes a raw 32-bit instruction word (as fetched from memory) and fills
    // in a DecodedInstr struct completely. Called once per instruction in the
    // ID stage.
    //
    // Returns a NOP (valid=false) for the all-zeros NOP instruction, and also
    // returns valid=false for any unrecognized encoding.
    // -----------------------------------------------------------------------
    static DecodedInstr decode(uint32_t raw_instr, uint32_t pc);

    // -----------------------------------------------------------------------
    // disassemble() — produce a human-readable assembly string.
    // Used for the pipeline visualization and debug output.
    // Example: "ADDI x10, x0, 42"
    // -----------------------------------------------------------------------
    static std::string disassemble(const DecodedInstr& d);

    // -----------------------------------------------------------------------
    // is_nop() — true if this is a structural NOP (ADDI x0, x0, 0 = 0x00000013)
    // -----------------------------------------------------------------------
    static bool is_nop(uint32_t raw);

private:
    // Field extractors — these mirror hardware bit-select logic
    static uint8_t  extract_opcode(uint32_t instr);
    static uint8_t  extract_rd    (uint32_t instr);
    static uint8_t  extract_rs1   (uint32_t instr);
    static uint8_t  extract_rs2   (uint32_t instr);
    static uint8_t  extract_funct3(uint32_t instr);
    static uint8_t  extract_funct7(uint32_t instr);

    // Immediate reconstruction — each format scrambles bits differently.
    // The hardware uses sign extension from the MSB (always bit 31).
    static int32_t  extract_imm_i (uint32_t instr); // I-type
    static int32_t  extract_imm_s (uint32_t instr); // S-type
    static int32_t  extract_imm_b (uint32_t instr); // B-type (branch offset)
    static int32_t  extract_imm_u (uint32_t instr); // U-type (upper 20 bits)
    static int32_t  extract_imm_j (uint32_t instr); // J-type (jump offset)

    // Control signal generators — one per opcode class
    static ControlSignals gen_ctrl_load  (uint8_t funct3);
    static ControlSignals gen_ctrl_store (uint8_t funct3);
    static ControlSignals gen_ctrl_branch(uint8_t funct3);
    static ControlSignals gen_ctrl_op_imm(uint8_t funct3, uint8_t funct7);
    static ControlSignals gen_ctrl_op    (uint8_t funct3, uint8_t funct7);
    static ControlSignals gen_ctrl_jal   ();
    static ControlSignals gen_ctrl_jalr  ();
    static ControlSignals gen_ctrl_lui   ();
    static ControlSignals gen_ctrl_auipc ();
    static ControlSignals gen_ctrl_system(uint32_t instr);

    // ABI register names (x0=zero, x1=ra, x2=sp, ...)
    static std::string reg_name(uint8_t idx);
};

} // namespace rv32i
