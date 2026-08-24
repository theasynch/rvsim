// =============================================================================
// hazard_unit.sv — Hazard Detection Unit
// =============================================================================
// Detects pipeline hazards and generates stall/flush control signals:
//
// 1. Load-Use Hazard:
//    If the instruction in EX is a LOAD and its rd matches rs1 or rs2 of
//    the instruction in ID, we must stall for one cycle (insert bubble).
//    - Stall PC (don't advance)
//    - Stall IF/ID register (re-decode same instruction)
//    - Flush ID/EX register (insert NOP bubble)
//
// 2. Control Hazard (Branch/Jump):
//    When a branch is taken or a jump executes in EX, the instructions
//    fetched after it (now in IF and ID) are wrong and must be flushed.
//    - Flush IF/ID register
//    - Flush ID/EX register
//
// =============================================================================

module hazard_unit
  import rv32i_pkg::*;
(
  // From ID/EX pipeline register
  input  logic        id_ex_mem_read,    // Is the instruction in EX a load?
  input  logic [4:0]  id_ex_rd,          // Destination register of EX instruction

  // From IF/ID pipeline register (instruction being decoded)
  input  logic [4:0]  if_id_rs1,         // Source register 1 of ID instruction
  input  logic [4:0]  if_id_rs2,         // Source register 2 of ID instruction

  // Branch/jump resolution from EX stage
  input  logic        branch_taken,      // Branch was taken
  input  logic        jump,              // JAL or JALR executing

  // Hazard control outputs
  output logic        stall_if,          // Freeze PC
  output logic        stall_id,          // Freeze IF/ID register
  output logic        flush_id,          // Clear IF/ID → NOP
  output logic        flush_ex           // Clear ID/EX → NOP
);

  logic load_use_hazard;

  // =========================================================================
  // Load-Use Hazard Detection
  // =========================================================================
  // A load-use hazard occurs when:
  //   - The instruction in EX is a load (mem_read = 1)
  //   - The load's destination register matches a source register of the
  //     instruction currently in ID
  //   - The destination is not x0 (writes to x0 are no-ops)
  assign load_use_hazard = id_ex_mem_read &&
                           (id_ex_rd != 5'b0) &&
                           ((id_ex_rd == if_id_rs1) || (id_ex_rd == if_id_rs2));

  // =========================================================================
  // Stall Signals (for load-use hazard)
  // =========================================================================
  assign stall_if = load_use_hazard;
  assign stall_id = load_use_hazard;

  // =========================================================================
  // Flush Signals
  // =========================================================================
  // Flush ID/EX on load-use hazard (insert bubble) OR on branch/jump taken
  assign flush_ex = load_use_hazard || branch_taken || jump;

  // Flush IF/ID on branch/jump taken (wrong-path instructions)
  assign flush_id = branch_taken || jump;

endmodule
