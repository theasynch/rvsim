// =============================================================================
// alu.sv — 32-bit Arithmetic Logic Unit
// =============================================================================
// Pure combinational module implementing all RV32I ALU operations.
//
// Operations:
//   ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND, PASS_B (for LUI)
//
// Inputs:  a (32-bit), b (32-bit), op (alu_op_t)
// Outputs: result (32-bit), zero (1-bit, high when result == 0)
// =============================================================================

module alu
  import rv32i_pkg::*;
(
  input  logic [31:0] a,
  input  logic [31:0] b,
  input  alu_op_t     op,

  output logic [31:0] result,
  output logic        zero
);

  always_comb begin
    case (op)
      ALU_ADD:    result = a + b;
      ALU_SUB:    result = a - b;
      ALU_SLL:    result = a << b[4:0];
      ALU_SLT:    result = {31'b0, $signed(a) < $signed(b)};
      ALU_SLTU:   result = {31'b0, a < b};
      ALU_XOR:    result = a ^ b;
      ALU_SRL:    result = a >> b[4:0];
      ALU_SRA:    result = $unsigned($signed(a) >>> b[4:0]);
      ALU_OR:     result = a | b;
      ALU_AND:    result = a & b;
      ALU_PASS_B: result = b;
      default:    result = 32'b0;
    endcase
  end

  assign zero = (result == 32'b0);

endmodule
