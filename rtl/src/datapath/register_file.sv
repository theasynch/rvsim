// =============================================================================
// register_file.sv — 32 × 32-bit Register File
// =============================================================================
// Two asynchronous read ports (rs1, rs2) and one synchronous write port (rd).
// Register x0 is hardwired to zero — writes to x0 are silently ignored.
//
// Write occurs on the NEGATIVE clock edge so that a value written in WB
// can be read in ID within the same cycle (textbook convention for
// eliminating one WB→ID forwarding case).
// =============================================================================

module register_file
  import rv32i_pkg::*;
(
  input  logic        clk,
  input  logic        rst_n,

  // Read ports (asynchronous / combinational)
  input  logic [4:0]  rs1_addr,
  input  logic [4:0]  rs2_addr,
  output logic [31:0] rs1_data,
  output logic [31:0] rs2_data,

  // Write port (synchronous, negedge)
  input  logic        wr_en,
  input  logic [4:0]  wr_addr,
  input  logic [31:0] wr_data
);

  // 32 registers, each 32 bits wide
  logic [31:0] regs [0:31];

  // --- Asynchronous reads ---
  // x0 always reads as zero; for other registers, if we are writing
  // to the same register this cycle, forward the write data (write-first).
  assign rs1_data = (rs1_addr == 5'b0) ? 32'b0 :
                    (wr_en && wr_addr == rs1_addr && wr_addr != 5'b0) ? wr_data :
                    regs[rs1_addr];

  assign rs2_data = (rs2_addr == 5'b0) ? 32'b0 :
                    (wr_en && wr_addr == rs2_addr && wr_addr != 5'b0) ? wr_data :
                    regs[rs2_addr];

  // --- Synchronous write on negative clock edge ---
  always_ff @(negedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int i = 0; i < 32; i++) begin
        regs[i] <= 32'b0;
      end
    end else if (wr_en && wr_addr != 5'b0) begin
      regs[wr_addr] <= wr_data;
    end
  end

endmodule
