// =============================================================================
// forwarding_unit.h — Data Forwarding (Bypassing) Unit
// =============================================================================
//
// Forwarding solves most RAW hazards WITHOUT stalling by routing the result
// of an earlier instruction's ALU stage directly into the inputs of a later
// instruction's ALU stage — bypassing the register file entirely.
//
// Two forwarding paths exist in the standard 5-stage pipeline:
//
//   EX/MEM → EX  (EX hazard):  instruction N-1's result → instruction N's ALU
//   MEM/WB → EX  (MEM hazard): instruction N-2's result → instruction N's ALU
//
// The forwarding unit is a 2-MUX block for each ALU input:
//
//       RegFile ──────────────────────────────┐
//       EX/MEM.ALUResult ────→  [2:1 MUX A] ──→ ALU_A
//       MEM/WB.WBData   ─────→                │
//                                              │
//       RegFile ──────────────────────────────┐
//       EX/MEM.ALUResult ────→  [2:1 MUX B] ──→ ALU_B
//       MEM/WB.WBData   ─────→              [IMM MUX]
//
// Priority: EX hazard takes priority over MEM hazard (closer instruction wins).
// =============================================================================
#pragma once

#include "isa/rv32i_types.h"

namespace rv32i {

struct ForwardingSignals {
    ForwardSel forward_a = ForwardSel::NONE; // MUX select for ALU input A (rs1)
    ForwardSel forward_b = ForwardSel::NONE; // MUX select for ALU input B (rs2)
};

class ForwardingUnit {
public:
    // -----------------------------------------------------------------------
    // compute() — purely combinational, called each cycle during EX stage.
    //
    // Compares the source registers of the instruction currently in EX
    // (stored in ID/EX) against the destination registers of instructions
    // in later stages (EX/MEM and MEM/WB).
    // -----------------------------------------------------------------------
    static ForwardingSignals compute(
        const IDEXReg&   id_ex,   // Current instruction in EX
        const EXMEMReg&  ex_mem,  // Instruction one stage ahead (just finished EX)
        const MEMWBReg&  mem_wb   // Instruction two stages ahead (just finished MEM)
    );
};

} // namespace rv32i
