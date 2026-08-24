// =============================================================================
// control_unit.sv — Main Control Unit + ALU Control
// =============================================================================
// Two-level decoder:
//
// 1. Main Decoder: opcode → high-level control signals
//    (reg_write, mem_read, mem_write, alu_src, branch, jump, etc.)
//
// 2. ALU Control: {alu_op_mode, funct3, funct7} → specific ALU operation
//
// This mirrors the control signal generation in the C++ simulator's
// ControlSignals struct and decoder.cpp.
// =============================================================================

module control_unit
  import rv32i_pkg::*;
(
  input  logic [31:0]   instruction,
  output ctrl_signals_t ctrl
);

  logic [6:0] opcode;
  logic [2:0] funct3;
  logic [6:0] funct7;

  assign opcode = instruction[6:0];
  assign funct3 = instruction[14:12];
  assign funct7 = instruction[31:25];

  // =========================================================================
  // Main Decoder — opcode → control signals
  // =========================================================================
  always_comb begin
    // Default: all signals off (NOP / invalid instruction)
    ctrl = '0;
    ctrl.funct3 = funct3;
    ctrl.funct7 = funct7;
    ctrl.alu_op = ALU_ADD; // default

    case (opcode)
      // -----------------------------------------------------------------
      // R-type: register-register operations (ADD, SUB, AND, OR, etc.)
      // -----------------------------------------------------------------
      OP_REG: begin
        ctrl.reg_write = 1'b1;
        ctrl.alu_src   = 1'b0;  // rs2 as ALU operand B
        ctrl.alu_op    = decode_r_type_alu(funct3, funct7);
      end

      // -----------------------------------------------------------------
      // I-type: immediate operations (ADDI, SLTI, ANDI, SLLI, etc.)
      // -----------------------------------------------------------------
      OP_IMM: begin
        ctrl.reg_write = 1'b1;
        ctrl.alu_src   = 1'b1;  // immediate as ALU operand B
        ctrl.alu_op    = decode_i_type_alu(funct3, funct7);
      end

      // -----------------------------------------------------------------
      // LOAD: LB, LH, LW, LBU, LHU
      // -----------------------------------------------------------------
      OP_LOAD: begin
        ctrl.reg_write  = 1'b1;
        ctrl.mem_read   = 1'b1;
        ctrl.mem_to_reg = 1'b1;  // WB mux selects memory data
        ctrl.alu_src    = 1'b1;  // address = rs1 + imm
        ctrl.alu_op     = ALU_ADD;
      end

      // -----------------------------------------------------------------
      // STORE: SB, SH, SW
      // -----------------------------------------------------------------
      OP_STORE: begin
        ctrl.mem_write = 1'b1;
        ctrl.alu_src   = 1'b1;  // address = rs1 + imm
        ctrl.alu_op    = ALU_ADD;
      end

      // -----------------------------------------------------------------
      // BRANCH: BEQ, BNE, BLT, BGE, BLTU, BGEU
      // -----------------------------------------------------------------
      OP_BRANCH: begin
        ctrl.branch = 1'b1;
        ctrl.alu_op = ALU_SUB;  // Used for comparison (result not stored)
      end

      // -----------------------------------------------------------------
      // JAL: Jump and Link
      // rd = PC+4, PC = PC + imm
      // -----------------------------------------------------------------
      OP_JAL: begin
        ctrl.reg_write = 1'b1;
        ctrl.jump      = 1'b1;
        ctrl.alu_op    = ALU_ADD;
      end

      // -----------------------------------------------------------------
      // JALR: Jump and Link Register
      // rd = PC+4, PC = (rs1 + imm) & ~1
      // -----------------------------------------------------------------
      OP_JALR: begin
        ctrl.reg_write = 1'b1;
        ctrl.jump      = 1'b1;
        ctrl.jalr      = 1'b1;
        ctrl.alu_src   = 1'b1;
        ctrl.alu_op    = ALU_ADD;
      end

      // -----------------------------------------------------------------
      // LUI: Load Upper Immediate
      // rd = imm << 12 (imm already shifted by imm_gen)
      // -----------------------------------------------------------------
      OP_LUI: begin
        ctrl.reg_write = 1'b1;
        ctrl.lui       = 1'b1;
        ctrl.alu_src   = 1'b1;
        ctrl.alu_op    = ALU_PASS_B;
      end

      // -----------------------------------------------------------------
      // AUIPC: Add Upper Immediate to PC
      // rd = PC + (imm << 12)
      // -----------------------------------------------------------------
      OP_AUIPC: begin
        ctrl.reg_write = 1'b1;
        ctrl.auipc     = 1'b1;
        ctrl.alu_src   = 1'b1;
        ctrl.alu_op    = ALU_ADD;
      end

      // -----------------------------------------------------------------
      // SYSTEM: ECALL / EBREAK — halt the processor
      // -----------------------------------------------------------------
      OP_SYSTEM: begin
        ctrl.is_ecall = 1'b1;
      end

      // -----------------------------------------------------------------
      // FENCE — treated as NOP
      // -----------------------------------------------------------------
      OP_FENCE: begin
        // No signals asserted — NOP
      end

      default: begin
        // Unknown opcode — treat as NOP
      end
    endcase
  end

  // =========================================================================
  // ALU Control — R-type: {funct3, funct7} → ALU operation
  // =========================================================================
  function automatic alu_op_t decode_r_type_alu(
    input logic [2:0] f3,
    input logic [6:0] f7
  );
    case (f3)
      F3_ADD_SUB: return (f7 == F7_ALT) ? ALU_SUB : ALU_ADD;
      F3_SLL:     return ALU_SLL;
      F3_SLT:     return ALU_SLT;
      F3_SLTU:    return ALU_SLTU;
      F3_XOR:     return ALU_XOR;
      F3_SRL_SRA: return (f7 == F7_ALT) ? ALU_SRA : ALU_SRL;
      F3_OR:      return ALU_OR;
      F3_AND:     return ALU_AND;
      default:    return ALU_ADD;
    endcase
  endfunction

  // =========================================================================
  // ALU Control — I-type: funct3 → ALU operation
  // (funct7 only matters for SRAI vs SRLI)
  // =========================================================================
  function automatic alu_op_t decode_i_type_alu(
    input logic [2:0] f3,
    input logic [6:0] f7
  );
    case (f3)
      F3_ADD_SUB: return ALU_ADD;   // ADDI (no SUBI in RV32I)
      F3_SLL:     return ALU_SLL;   // SLLI
      F3_SLT:     return ALU_SLT;   // SLTI
      F3_SLTU:    return ALU_SLTU;  // SLTIU
      F3_XOR:     return ALU_XOR;   // XORI
      F3_SRL_SRA: return (f7 == F7_ALT) ? ALU_SRA : ALU_SRL;  // SRAI vs SRLI
      F3_OR:      return ALU_OR;    // ORI
      F3_AND:     return ALU_AND;   // ANDI
      default:    return ALU_ADD;
    endcase
  endfunction

endmodule
