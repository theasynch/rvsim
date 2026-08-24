// =============================================================================
// branch_comp.sv — Branch Comparator
// =============================================================================
// Evaluates all six RV32I branch conditions:
//   BEQ  (funct3=000): taken if rs1 == rs2
//   BNE  (funct3=001): taken if rs1 != rs2
//   BLT  (funct3=100): taken if rs1 <  rs2  (signed)
//   BGE  (funct3=101): taken if rs1 >= rs2  (signed)
//   BLTU (funct3=110): taken if rs1 <  rs2  (unsigned)
//   BGEU (funct3=111): taken if rs1 >= rs2  (unsigned)
//
// Output: branch_taken (1 = branch should be taken)
// =============================================================================

module branch_comp
  import rv32i_pkg::*;
(
  input  logic [31:0] rs1_data,
  input  logic [31:0] rs2_data,
  input  logic [2:0]  funct3,
  output logic        branch_taken
);

  always_comb begin
    case (funct3)
      F3_BEQ:  branch_taken = (rs1_data == rs2_data);
      F3_BNE:  branch_taken = (rs1_data != rs2_data);
      F3_BLT:  branch_taken = ($signed(rs1_data) <  $signed(rs2_data));
      F3_BGE:  branch_taken = ($signed(rs1_data) >= $signed(rs2_data));
      F3_BLTU: branch_taken = (rs1_data <  rs2_data);
      F3_BGEU: branch_taken = (rs1_data >= rs2_data);
      default: branch_taken = 1'b0;
    endcase
  end

endmodule
