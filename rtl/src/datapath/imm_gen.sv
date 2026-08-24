// =============================================================================
// imm_gen.sv — Immediate Generator
// =============================================================================
// Extracts and sign-extends the immediate value from a 32-bit RISC-V
// instruction. Supports all six instruction formats:
//
//   I-type: imm[11:0]                        (loads, ADDI, JALR, etc.)
//   S-type: {imm[11:5], imm[4:0]}            (stores)
//   B-type: {imm[12], imm[10:5], imm[4:1], 0} (branches)
//   U-type: {imm[31:12], 12'b0}              (LUI, AUIPC)
//   J-type: {imm[20], imm[10:1], imm[11], imm[19:12], 0} (JAL)
//
// The format is determined by the opcode field (bits [6:0]).
// =============================================================================

module imm_gen
  import rv32i_pkg::*;
(
  input  logic [31:0] instruction,
  output logic [31:0] imm
);

  logic [6:0] opcode;
  assign opcode = instruction[6:0];

  always_comb begin
    case (opcode)
      // -----------------------------------------------------------------
      // I-type: LOAD, OP_IMM, JALR, SYSTEM/FENCE
      // imm[11:0] = instruction[31:20]
      // -----------------------------------------------------------------
      OP_LOAD, OP_IMM, OP_JALR, OP_SYSTEM, OP_FENCE:
        imm = {{20{instruction[31]}}, instruction[31:20]};

      // -----------------------------------------------------------------
      // S-type: STORE
      // imm[11:5] = instruction[31:25], imm[4:0] = instruction[11:7]
      // -----------------------------------------------------------------
      OP_STORE:
        imm = {{20{instruction[31]}}, instruction[31:25], instruction[11:7]};

      // -----------------------------------------------------------------
      // B-type: BRANCH
      // imm[12|10:5|4:1|11] → {inst[31], inst[7], inst[30:25], inst[11:8], 0}
      // -----------------------------------------------------------------
      OP_BRANCH:
        imm = {{19{instruction[31]}}, instruction[31], instruction[7],
                instruction[30:25], instruction[11:8], 1'b0};

      // -----------------------------------------------------------------
      // U-type: LUI, AUIPC
      // imm[31:12] = instruction[31:12], lower 12 bits = 0
      // -----------------------------------------------------------------
      OP_LUI, OP_AUIPC:
        imm = {instruction[31:12], 12'b0};

      // -----------------------------------------------------------------
      // J-type: JAL
      // imm[20|10:1|11|19:12] → {inst[31], inst[19:12], inst[20], inst[30:21], 0}
      // -----------------------------------------------------------------
      OP_JAL:
        imm = {{11{instruction[31]}}, instruction[31], instruction[19:12],
                instruction[20], instruction[30:21], 1'b0};

      default:
        imm = 32'b0;
    endcase
  end

endmodule
