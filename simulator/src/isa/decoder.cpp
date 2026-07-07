// =============================================================================
// decoder.cpp — RISC-V RV32I Instruction Decoder Implementation
// =============================================================================
#include "isa/decoder.h"
#include <sstream>
#include <stdexcept>

namespace rv32i {

// ---------------------------------------------------------------------------
// ABI register names (used in disassembly)
// ---------------------------------------------------------------------------
static const char* ABI_NAMES[32] = {
    "zero","ra","sp","gp","tp","t0","t1","t2",
    "s0",  "s1","a0","a1","a2","a3","a4","a5",
    "a6",  "a7","s2","s3","s4","s5","s6","s7",
    "s8",  "s9","s10","s11","t3","t4","t5","t6"
};

std::string Decoder::reg_name(uint8_t idx) {
    if (idx < 32) return std::string(ABI_NAMES[idx]);
    return "x?";
}

// ---------------------------------------------------------------------------
// Field extractors — mirror hardware bit-select operations
// ---------------------------------------------------------------------------
uint8_t Decoder::extract_opcode(uint32_t i) { return (i >> 0)  & 0x7F; }
uint8_t Decoder::extract_rd    (uint32_t i) { return (i >> 7)  & 0x1F; }
uint8_t Decoder::extract_funct3(uint32_t i) { return (i >> 12) & 0x07; }
uint8_t Decoder::extract_rs1   (uint32_t i) { return (i >> 15) & 0x1F; }
uint8_t Decoder::extract_rs2   (uint32_t i) { return (i >> 20) & 0x1F; }
uint8_t Decoder::extract_funct7(uint32_t i) { return (i >> 25) & 0x7F; }

// ---------------------------------------------------------------------------
// Immediate extraction — each format arranges bits differently to keep
// rs1, rs2, and rd in the same positions across all formats (hardware trick).
//
// Sign extension: RISC-V always uses bit 31 as the sign bit.
// In C++: right-shift of int32_t sign-extends; we cast then shift.
// ---------------------------------------------------------------------------

// I-type: imm = instr[31:20], sign-extended
int32_t Decoder::extract_imm_i(uint32_t i) {
    return (int32_t)i >> 20; // arithmetic shift sign-extends
}

// S-type: imm = {instr[31:25], instr[11:7]}, sign-extended
int32_t Decoder::extract_imm_s(uint32_t i) {
    int32_t imm = ((int32_t)i >> 20) & 0xFFFFFFE0; // bits [31:25] → [11:5]
    imm |= (i >> 7) & 0x1F;                          // bits [11:7]  → [4:0]
    return imm;
}

// B-type: imm = {instr[31], instr[7], instr[30:25], instr[11:8], 0}
// Note: bit 0 is always 0 (branches are 2-byte aligned in RV32I)
int32_t Decoder::extract_imm_b(uint32_t i) {
    int32_t imm = 0;
    imm |= ((i >> 31) & 0x1) << 12; // bit 12 (sign)
    imm |= ((i >> 7)  & 0x1) << 11; // bit 11
    imm |= ((i >> 25) & 0x3F) << 5; // bits [10:5]
    imm |= ((i >> 8)  & 0xF)  << 1; // bits [4:1]
    // Sign extend from bit 12
    if (imm & 0x1000) imm |= 0xFFFFE000;
    return imm;
}

// U-type: imm = instr[31:12] << 12 (bits [11:0] are zero)
int32_t Decoder::extract_imm_u(uint32_t i) {
    return (int32_t)(i & 0xFFFFF000); // upper 20 bits, lower 12 cleared
}

// J-type: imm = {instr[31], instr[19:12], instr[20], instr[30:21], 0}
// Scrambled bit order for JAL (same rd-field position trick as B-type)
int32_t Decoder::extract_imm_j(uint32_t i) {
    int32_t imm = 0;
    imm |= ((i >> 31) & 0x1)  << 20; // bit 20 (sign)
    imm |= ((i >> 12) & 0xFF) << 12; // bits [19:12]
    imm |= ((i >> 20) & 0x1)  << 11; // bit 11
    imm |= ((i >> 21) & 0x3FF) << 1; // bits [10:1]
    // Sign extend from bit 20
    if (imm & 0x100000) imm |= 0xFFE00000;
    return imm;
}

// ---------------------------------------------------------------------------
// Control signal generators — one per opcode group
// ---------------------------------------------------------------------------
ControlSignals Decoder::gen_ctrl_load(uint8_t funct3) {
    ControlSignals c;
    c.reg_write  = true;
    c.mem_read   = true;
    c.mem_to_reg = true; // WB selects memory data, not ALU result
    c.alu_src    = true; // Address = rs1 + imm (ALU uses immediate)
    c.alu_op     = ALUOp::ADD;
    switch (funct3) {
        case 0x0: c.mem_size = MemSize::BYTE;     c.mem_signed = true;  break; // LB
        case 0x1: c.mem_size = MemSize::HALFWORD; c.mem_signed = true;  break; // LH
        case 0x2: c.mem_size = MemSize::WORD;     c.mem_signed = true;  break; // LW
        case 0x4: c.mem_size = MemSize::BYTE;     c.mem_signed = false; break; // LBU
        case 0x5: c.mem_size = MemSize::HALFWORD; c.mem_signed = false; break; // LHU
    }
    return c;
}

ControlSignals Decoder::gen_ctrl_store(uint8_t funct3) {
    ControlSignals c;
    c.mem_write = true;
    c.alu_src   = true; // Address = rs1 + imm
    c.alu_op    = ALUOp::ADD;
    switch (funct3) {
        case 0x0: c.mem_size = MemSize::BYTE;     break; // SB
        case 0x1: c.mem_size = MemSize::HALFWORD; break; // SH
        case 0x2: c.mem_size = MemSize::WORD;     break; // SW
    }
    return c;
}

ControlSignals Decoder::gen_ctrl_branch(uint8_t funct3) {
    ControlSignals c;
    c.branch = true;
    // ALU computes the comparison; branch unit evaluates result
    switch (funct3) {
        case 0x0: c.alu_op = ALUOp::SUB;  break; // BEQ (zero flag after SUB)
        case 0x1: c.alu_op = ALUOp::SUB;  break; // BNE
        case 0x4: c.alu_op = ALUOp::SLT;  break; // BLT
        case 0x5: c.alu_op = ALUOp::SLT;  break; // BGE
        case 0x6: c.alu_op = ALUOp::SLTU; break; // BLTU
        case 0x7: c.alu_op = ALUOp::SLTU; break; // BGEU
        default:  c.alu_op = ALUOp::SUB;  break;
    }
    return c;
}

ControlSignals Decoder::gen_ctrl_op_imm(uint8_t funct3, uint8_t funct7) {
    ControlSignals c;
    c.reg_write = true;
    c.alu_src   = true; // B operand is immediate
    switch (funct3) {
        case 0x0: c.alu_op = ALUOp::ADD;  break; // ADDI
        case 0x2: c.alu_op = ALUOp::SLT;  break; // SLTI
        case 0x3: c.alu_op = ALUOp::SLTU; break; // SLTIU
        case 0x4: c.alu_op = ALUOp::XOR;  break; // XORI
        case 0x6: c.alu_op = ALUOp::OR;   break; // ORI
        case 0x7: c.alu_op = ALUOp::AND;  break; // ANDI
        case 0x1: c.alu_op = ALUOp::SLL;  break; // SLLI
        case 0x5:
            c.alu_op = (funct7 == 0x20) ? ALUOp::SRA : ALUOp::SRL; // SRAI vs SRLI
            break;
    }
    return c;
}

ControlSignals Decoder::gen_ctrl_op(uint8_t funct3, uint8_t funct7) {
    ControlSignals c;
    c.reg_write = true;
    // alu_src = false (both operands from register file)
    switch (funct3) {
        case 0x0: c.alu_op = (funct7 == 0x20) ? ALUOp::SUB : ALUOp::ADD; break;
        case 0x1: c.alu_op = ALUOp::SLL;  break;
        case 0x2: c.alu_op = ALUOp::SLT;  break;
        case 0x3: c.alu_op = ALUOp::SLTU; break;
        case 0x4: c.alu_op = ALUOp::XOR;  break;
        case 0x5: c.alu_op = (funct7 == 0x20) ? ALUOp::SRA : ALUOp::SRL; break;
        case 0x6: c.alu_op = ALUOp::OR;   break;
        case 0x7: c.alu_op = ALUOp::AND;  break;
    }
    return c;
}

ControlSignals Decoder::gen_ctrl_jal() {
    ControlSignals c;
    c.reg_write = true;  // rd = PC+4 (return address)
    c.jump      = true;
    c.alu_op    = ALUOp::ADD; // PC + imm
    return c;
}

ControlSignals Decoder::gen_ctrl_jalr() {
    ControlSignals c;
    c.reg_write = true;
    c.jalr      = true;
    c.alu_src   = true;
    c.alu_op    = ALUOp::ADD; // rs1 + imm (AND'd with ~1)
    return c;
}

ControlSignals Decoder::gen_ctrl_lui() {
    ControlSignals c;
    c.reg_write = true;
    c.lui       = true;
    c.alu_op    = ALUOp::PASS_B; // Just pass the immediate
    return c;
}

ControlSignals Decoder::gen_ctrl_auipc() {
    ControlSignals c;
    c.reg_write = true;
    c.auipc     = true;
    c.alu_op    = ALUOp::ADD; // PC + upper_imm
    return c;
}

ControlSignals Decoder::gen_ctrl_system(uint32_t instr) {
    ControlSignals c;
    // Only ECALL (funct12=0) is treated specially — it halts the simulator
    uint32_t funct12 = (instr >> 20) & 0xFFF;
    if (funct12 == 0) c.is_ecall = true;
    return c;
}

// ---------------------------------------------------------------------------
// Main decode() — orchestrates field extraction + control generation
// ---------------------------------------------------------------------------
DecodedInstr Decoder::decode(uint32_t raw, uint32_t pc) {
    DecodedInstr d;
    d.raw     = raw;
    d.valid   = true;

    uint8_t op7    = extract_opcode(raw);
    d.rd           = extract_rd    (raw);
    d.rs1          = extract_rs1   (raw);
    d.rs2          = extract_rs2   (raw);
    d.funct3       = extract_funct3(raw);
    d.funct7       = extract_funct7(raw);

    // Determine opcode enum
    switch (op7) {
        case 0b000'0011: d.opcode = Opcode::LOAD;   break;
        case 0b010'0011: d.opcode = Opcode::STORE;  break;
        case 0b110'0011: d.opcode = Opcode::BRANCH; break;
        case 0b110'0111: d.opcode = Opcode::JALR;   break;
        case 0b110'1111: d.opcode = Opcode::JAL;    break;
        case 0b001'0011: d.opcode = Opcode::OP_IMM; break;
        case 0b011'0011: d.opcode = Opcode::OP;     break;
        case 0b011'0111: d.opcode = Opcode::LUI;    break;
        case 0b001'0111: d.opcode = Opcode::AUIPC;  break;
        case 0b111'0011: d.opcode = Opcode::SYSTEM; break;
        case 0b000'1111: d.opcode = Opcode::FENCE;  break;
        default:
            // All-zeros = NOP; other invalid encodings become invalid
            d.opcode = Opcode::INVALID;
            d.valid  = (raw == 0x00000013); // canonical NOP is valid but no-op
            if (raw != 0 && raw != 0x00000013) d.valid = false;
            return d;
    }

    // Extract immediate and format based on opcode
    switch (d.opcode) {
        case Opcode::OP:
            d.format = InstrFormat::R;
            d.imm    = 0;
            d.ctrl   = gen_ctrl_op(d.funct3, d.funct7);
            break;
        case Opcode::LOAD:
        case Opcode::OP_IMM:
        case Opcode::JALR:
            d.format = InstrFormat::I;
            d.imm    = extract_imm_i(raw);
            if      (d.opcode == Opcode::LOAD)   d.ctrl = gen_ctrl_load(d.funct3);
            else if (d.opcode == Opcode::OP_IMM) d.ctrl = gen_ctrl_op_imm(d.funct3, d.funct7);
            else                                  d.ctrl = gen_ctrl_jalr();
            break;
        case Opcode::STORE:
            d.format = InstrFormat::S;
            d.imm    = extract_imm_s(raw);
            d.ctrl   = gen_ctrl_store(d.funct3);
            break;
        case Opcode::BRANCH:
            d.format = InstrFormat::B;
            d.imm    = extract_imm_b(raw);
            d.ctrl   = gen_ctrl_branch(d.funct3);
            break;
        case Opcode::LUI:
        case Opcode::AUIPC:
            d.format = InstrFormat::U;
            d.imm    = extract_imm_u(raw);
            d.ctrl   = (d.opcode == Opcode::LUI) ? gen_ctrl_lui() : gen_ctrl_auipc();
            break;
        case Opcode::JAL:
            d.format = InstrFormat::J;
            d.imm    = extract_imm_j(raw);
            d.ctrl   = gen_ctrl_jal();
            break;
        case Opcode::SYSTEM:
            d.format = InstrFormat::I;
            d.imm    = extract_imm_i(raw);
            d.ctrl   = gen_ctrl_system(raw);
            break;
        case Opcode::FENCE:
            d.format = InstrFormat::I;
            d.imm    = 0; // treated as NOP
            break;
        default:
            d.valid = false;
            return d;
    }

    d.mnemonic = disassemble(d);
    return d;
}

// ---------------------------------------------------------------------------
// disassemble() — produce human-readable instruction string
// ---------------------------------------------------------------------------
std::string Decoder::disassemble(const DecodedInstr& d) {
    if (!d.valid) return "NOP";
    std::ostringstream s;
    std::string rd  = reg_name(d.rd);
    std::string rs1 = reg_name(d.rs1);
    std::string rs2 = reg_name(d.rs2);

    switch (d.opcode) {
        case Opcode::OP:
            switch (d.funct3) {
                case 0x0: s << (d.funct7==0x20?"SUB":"ADD"); break;
                case 0x1: s << "SLL";  break; case 0x2: s << "SLT";  break;
                case 0x3: s << "SLTU"; break; case 0x4: s << "XOR";  break;
                case 0x5: s << (d.funct7==0x20?"SRA":"SRL"); break;
                case 0x6: s << "OR";   break; case 0x7: s << "AND";  break;
            }
            s << " " << rd << ", " << rs1 << ", " << rs2;
            break;
        case Opcode::OP_IMM:
            switch (d.funct3) {
                case 0x0: s << "ADDI";  break; case 0x2: s << "SLTI";  break;
                case 0x3: s << "SLTIU"; break; case 0x4: s << "XORI";  break;
                case 0x6: s << "ORI";   break; case 0x7: s << "ANDI";  break;
                case 0x1: s << "SLLI";  break;
                case 0x5: s << (d.funct7==0x20?"SRAI":"SRLI"); break;
            }
            s << " " << rd << ", " << rs1 << ", " << d.imm;
            break;
        case Opcode::LOAD:
            switch (d.funct3) {
                case 0x0: s << "LB";  break; case 0x1: s << "LH";  break;
                case 0x2: s << "LW";  break; case 0x4: s << "LBU"; break;
                case 0x5: s << "LHU"; break;
            }
            s << " " << rd << ", " << d.imm << "(" << rs1 << ")";
            break;
        case Opcode::STORE:
            switch (d.funct3) {
                case 0x0: s << "SB"; break; case 0x1: s << "SH"; break;
                case 0x2: s << "SW"; break;
            }
            s << " " << rs2 << ", " << d.imm << "(" << rs1 << ")";
            break;
        case Opcode::BRANCH:
            switch (d.funct3) {
                case 0x0: s << "BEQ";  break; case 0x1: s << "BNE";  break;
                case 0x4: s << "BLT";  break; case 0x5: s << "BGE";  break;
                case 0x6: s << "BLTU"; break; case 0x7: s << "BGEU"; break;
            }
            s << " " << rs1 << ", " << rs2 << ", " << std::showpos << d.imm << std::noshowpos;
            break;
        case Opcode::JAL:
            s << "JAL " << rd << ", " << std::showpos << d.imm << std::noshowpos;
            break;
        case Opcode::JALR:
            s << "JALR " << rd << ", " << d.imm << "(" << rs1 << ")";
            break;
        case Opcode::LUI:
            s << "LUI " << rd << ", 0x" << std::hex << ((d.imm >> 12) & 0xFFFFF) << std::dec;
            break;
        case Opcode::AUIPC:
            s << "AUIPC " << rd << ", 0x" << std::hex << ((d.imm >> 12) & 0xFFFFF) << std::dec;
            break;
        case Opcode::SYSTEM:
            s << (d.ctrl.is_ecall ? "ECALL" : "EBREAK");
            break;
        case Opcode::FENCE:
            s << "FENCE";
            break;
        default:
            s << "???";
    }
    return s.str();
}

bool Decoder::is_nop(uint32_t raw) {
    return raw == 0x00000013 || raw == 0;
}

} // namespace rv32i
