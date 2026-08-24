// =============================================================================
// forwarding_unit.sv — Data Forwarding (Bypassing) Unit
// =============================================================================
// Detects RAW (Read-After-Write) data hazards and generates MUX select
// signals to forward results from EX/MEM or MEM/WB pipeline registers
// back to the EX stage inputs.
//
// Priority: EX/MEM forward takes precedence over MEM/WB forward
// (more recent instruction has the correct value).
//
// This is the hardware equivalent of forwarding_unit.h/cpp from the
// C++ simulator.
// =============================================================================

module forwarding_unit
  import rv32i_pkg::*;
(
  // EX stage source registers (from ID/EX pipeline register)
  input  logic [4:0]  id_ex_rs1,
  input  logic [4:0]  id_ex_rs2,

  // EX/MEM pipeline register outputs
  input  logic        ex_mem_reg_write,
  input  logic [4:0]  ex_mem_rd,

  // MEM/WB pipeline register outputs
  input  logic        mem_wb_reg_write,
  input  logic [4:0]  mem_wb_rd,

  // Forwarding MUX select signals
  output forward_sel_t forward_a,   // For ALU operand A (rs1)
  output forward_sel_t forward_b    // For ALU operand B (rs2)
);

  // =========================================================================
  // Forward A (rs1 path)
  // =========================================================================
  always_comb begin
    // Default: no forwarding
    forward_a = FWD_NONE;

    // EX hazard (1-cycle-old result) — highest priority
    if (ex_mem_reg_write &&
        ex_mem_rd != 5'b0 &&
        ex_mem_rd == id_ex_rs1) begin
      forward_a = FWD_EX_MEM;
    end
    // MEM hazard (2-cycle-old result) — only if EX hazard doesn't apply
    else if (mem_wb_reg_write &&
             mem_wb_rd != 5'b0 &&
             mem_wb_rd == id_ex_rs1) begin
      forward_a = FWD_MEM_WB;
    end
  end

  // =========================================================================
  // Forward B (rs2 path)
  // =========================================================================
  always_comb begin
    // Default: no forwarding
    forward_b = FWD_NONE;

    // EX hazard — highest priority
    if (ex_mem_reg_write &&
        ex_mem_rd != 5'b0 &&
        ex_mem_rd == id_ex_rs2) begin
      forward_b = FWD_EX_MEM;
    end
    // MEM hazard — lower priority
    else if (mem_wb_reg_write &&
             mem_wb_rd != 5'b0 &&
             mem_wb_rd == id_ex_rs2) begin
      forward_b = FWD_MEM_WB;
    end
  end

endmodule
