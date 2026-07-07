// =============================================================================
// hazard_unit.h — Pipeline Hazard Detection Unit
// =============================================================================
//
// In a real CPU, this is purely combinational logic that looks at the current
// pipeline register contents and asserts stall/flush control signals.
//
// THREE types of hazards exist:
//
// 1. DATA HAZARDS (RAW — Read After Write):
//    Instruction B tries to read a register that instruction A (earlier) hasn't
//    written yet. In our 5-stage pipeline:
//    - IF-ID: 0 cycles stall (not possible yet)
//    - ID-EX: 0-2 cycles stall depending on forwarding availability
//    - Special case: LOAD-USE hazard (LW then immediate use) always needs 1 stall
//
// 2. CONTROL HAZARDS (Branch/Jump):
//    We don't know the branch outcome until EX stage, but we've already fetched
//    2 instructions in IF and ID. If branch is taken, those 2 instructions must
//    be flushed (turned into NOPs/bubbles).
//    → Our design: always predict NOT-TAKEN, flush on misprediction.
//    → Configurable: pluggable branch predictor can reduce flushes.
//
// 3. STRUCTURAL HAZARDS:
//    In RV32I with separate I-cache and D-cache, structural hazards are minimal.
//    We don't model memory access conflicts in the base pipeline.
// =============================================================================
#pragma once

#include "isa/rv32i_types.h"

namespace rv32i {

struct HazardSignals {
    bool stall_if    = false; // PCWrite = 0: don't advance IF
    bool stall_id    = false; // IF_ID.Write = 0: freeze IF/ID register
    bool flush_id    = false; // Turn ID/EX into NOP (insert bubble after ID)
    bool flush_ex    = false; // Turn EX/MEM into NOP (flush speculative EX)
    bool flush_if    = false; // Turn IF/ID into NOP (flush wrongly-fetched instr)
    bool load_use    = false; // Set when a load-use stall is injected (for stats)
    bool ctrl_hazard = false; // Set when branch/jump flushes (for stats)
};

class HazardUnit {
public:
    // -----------------------------------------------------------------------
    // detect() — combinational logic, called once per cycle BEFORE stages run.
    //
    // Takes:
    //   id_ex  — what's currently in the ID/EX pipeline register (EXECUTING)
    //   if_id  — what's currently in the IF/ID pipeline register (DECODING)
    //   branch_taken — output from EX stage branch evaluator this cycle
    //
    // Returns control signals to be applied before any stage updates.
    // -----------------------------------------------------------------------
    static HazardSignals detect(
        const IDEXReg& id_ex,
        const IFIDReg& if_id,
        bool branch_taken
    );

private:
    // Extract rs1 and rs2 from a raw instruction word (for load-use detection)
    static uint8_t rs1_from_raw(uint32_t raw);
    static uint8_t rs2_from_raw(uint32_t raw);
};

} // namespace rv32i
