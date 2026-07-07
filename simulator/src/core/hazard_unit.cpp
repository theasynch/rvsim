// hazard_unit.cpp
#include "core/hazard_unit.h"

namespace rv32i {

uint8_t HazardUnit::rs1_from_raw(uint32_t raw) { return (raw >> 15) & 0x1F; }
uint8_t HazardUnit::rs2_from_raw(uint32_t raw) { return (raw >> 20) & 0x1F; }

HazardSignals HazardUnit::detect(
    const IDEXReg& id_ex,
    const IFIDReg& if_id,
    bool branch_taken)
{
    HazardSignals h;

    // -----------------------------------------------------------------------
    // LOAD-USE HAZARD DETECTION
    //
    // Condition: The instruction in EX (id_ex) is a LOAD, AND the instruction
    // currently in ID (if_id) reads the register that the load writes.
    //
    // Timeline:
    //   Cycle N:   LW rd, imm(rs1)  [ID stage → will be in EX next cycle]
    //   Cycle N+1: INST uses rd      [IF stage → will be in ID next cycle]
    //
    // We detect this BEFORE cycle N+1's EX runs, so we stall by:
    //   - Keeping IF frozen (don't fetch next instruction)
    //   - Keeping IF/ID frozen (don't decode next instruction)
    //   - Inserting a bubble (NOP) into ID/EX
    // -----------------------------------------------------------------------
    if (id_ex.valid && id_ex.ctrl.mem_read) {
        uint8_t if_rs1 = rs1_from_raw(if_id.instruction);
        uint8_t if_rs2 = rs2_from_raw(if_id.instruction);
        uint8_t if_op  = if_id.instruction & 0x7F;

        // Check if the instruction in ID actually uses the load destination
        bool uses_rs1 = (id_ex.rd != 0) && (id_ex.rd == if_rs1);

        // S-type and B-type also read rs2; R-type reads rs2; others may not
        // We check opcode to decide if rs2 matters
        bool needs_rs2 = (if_op == 0b0110011 || // R-type
                          if_op == 0b0100011 || // S-type
                          if_op == 0b1100011);  // B-type
        bool uses_rs2 = needs_rs2 && (id_ex.rd != 0) && (id_ex.rd == if_rs2);

        if (uses_rs1 || uses_rs2) {
            h.stall_if = true;
            h.stall_id = true;
            h.flush_id = true; // Insert bubble into EX (ID/EX ← NOP)
            h.load_use = true;
        }
    }

    // -----------------------------------------------------------------------
    // CONTROL HAZARD: Branch or Jump taken
    //
    // When a branch/jump resolves as taken in EX stage, the two instructions
    // already fetched (in IF and ID) are WRONG. We flush them by turning
    // IF/ID and ID/EX into bubbles.
    //
    // Note: We don't stall — the pipeline keeps running, but we zero out the
    // speculative instructions. This costs exactly 2 cycles (the branch penalty).
    //
    // A branch predictor reduces this penalty by guessing in advance.
    // -----------------------------------------------------------------------
    if (branch_taken && !h.stall_if) { // Don't flush if we're already stalling
        h.flush_if    = true; // Flush IF/ID (instruction fetched after branch)
        h.flush_ex    = true; // Flush ID/EX (instruction decoded after branch)
        h.ctrl_hazard = true;
    }

    return h;
}

} // namespace rv32i
