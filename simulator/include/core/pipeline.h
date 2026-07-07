// =============================================================================
// pipeline.h — 5-Stage In-Order RISC-V Pipeline
// =============================================================================
//
// Models the classic textbook pipeline:
//
//   IF → ID → EX → MEM → WB
//
// Each stage is a function called once per clock cycle. The pipeline registers
// (IFIDReg, IDEXReg, EXMEMReg, MEMWBReg) are the "state" between stages.
//
// The Pipeline class owns all simulator state and is the main API surface
// used by the HTTP server.
// =============================================================================
#pragma once

#include "isa/rv32i_types.h"
#include "isa/decoder.h"
#include "core/register_file.h"
#include "core/memory.h"
#include "core/hazard_unit.h"
#include "core/forwarding_unit.h"
#include "branch/predictor.h"
#include "cache/cache.h"
#include "stats/perf_counters.h"
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace rv32i {

// ---------------------------------------------------------------------------
// VisualizationSnapshot — what the frontend needs to render one cycle
// ---------------------------------------------------------------------------
struct StageSnapshot {
    std::string stage;      // "IF", "ID", "EX", "MEM", "WB"
    uint32_t    pc      = 0;
    std::string pc_hex;
    std::string asm_str;    // e.g. "ADD x3, x1, x2"
    bool        bubble  = false;
    bool        stalled = false;
    // EX stage extras
    std::string forward_a;  // "EX_MEM", "MEM_WB", or "NONE"
    std::string forward_b;
    // MEM stage extras
    bool        cache_hit  = false;
    bool        is_mem_op  = false;
    bool        is_write   = false;
    // WB stage extras
    int         wb_rd      = 0;
    int32_t     wb_value   = 0;
    bool        wb_active  = false;
};

struct PipelineState {
    uint64_t    cycle;
    bool        halted;
    bool        stall_this_cycle;
    bool        flush_this_cycle;
    StageSnapshot stages[5]; // IF=0, ID=1, EX=2, MEM=3, WB=4
    PerfCounters  stats;
    nlohmann::json registers;
    nlohmann::json cache_state;
    nlohmann::json predictor_state;
};

// ---------------------------------------------------------------------------
// Pipeline configuration — pluggable components
// ---------------------------------------------------------------------------
struct PipelineConfig {
    std::string predictor_type = "static_not_taken"; // "static_taken", "static_not_taken", "two_bit", "gshare", "tournament"
    std::string cache_config   = "default";          // JSON string with cache params
    bool        forwarding_enabled = true;           // Turn off to see stalls without forwarding
    bool        hazard_detection   = true;           // Turn off to see incorrect execution
};

// ---------------------------------------------------------------------------
// Pipeline class — the main simulator
// ---------------------------------------------------------------------------
class Pipeline {
public:
    explicit Pipeline(PipelineConfig cfg = {});

    // -----------------------------------------------------------------------
    // step() — advance the pipeline by exactly ONE clock cycle.
    // Returns a PipelineState snapshot for the frontend.
    // -----------------------------------------------------------------------
    PipelineState step();

    // -----------------------------------------------------------------------
    // run() — advance N cycles (or until halted). Returns final state.
    // -----------------------------------------------------------------------
    PipelineState run(int max_cycles = 100000);

    // -----------------------------------------------------------------------
    // load_program() — write hex text into memory and reset pipeline state.
    // -----------------------------------------------------------------------
    bool load_program(const std::string& hex_str);

    // -----------------------------------------------------------------------
    // reset() — clear pipeline registers and stats, keep memory
    // -----------------------------------------------------------------------
    void reset();

    // -----------------------------------------------------------------------
    // get_state() — get current snapshot without advancing
    // -----------------------------------------------------------------------
    PipelineState get_state() const;

    // -----------------------------------------------------------------------
    // configure() — hot-swap predictor and cache config
    // -----------------------------------------------------------------------
    void configure(const PipelineConfig& cfg);

    bool is_halted() const { return halted_; }
    uint64_t cycle_count() const { return cycle_; }

private:
    // Pipeline registers (current and next values)
    IFIDReg  if_id_,  if_id_next_;
    IDEXReg  id_ex_,  id_ex_next_;
    EXMEMReg ex_mem_, ex_mem_next_;
    MEMWBReg mem_wb_, mem_wb_next_;

    uint32_t pc_;
    uint64_t cycle_;
    bool     halted_;

    // Submodules
    RegisterFile        regs_;
    Memory              mem_;
    std::unique_ptr<Predictor> predictor_;
    std::unique_ptr<Cache>     icache_;
    std::unique_ptr<Cache>     dcache_;
    PerfCounters                stats_;
    PipelineConfig              config_;
    Decoder                     decoder_;

    // Last cycle visualization (for snapshot without stepping)
    mutable PipelineState last_state_;

    // -----------------------------------------------------------------------
    // Stage implementations — each runs one cycle of its stage
    // -----------------------------------------------------------------------
    void stage_wb  (StageSnapshot& snap);
    void stage_mem (StageSnapshot& snap);
    void stage_ex  (StageSnapshot& snap, const ForwardingSignals& fwd);
    void stage_id  (StageSnapshot& snap, const HazardSignals& hz);
    void stage_if  (StageSnapshot& snap, const HazardSignals& hz);

    // ALU
    int32_t alu_execute(ALUOp op, int32_t a, int32_t b) const;

    // Branch evaluation: given the ALU result and funct3, did the branch fire?
    bool eval_branch(uint8_t funct3, int32_t alu_result, int32_t rs1, int32_t rs2) const;

    // Latch next values into current registers (at end of cycle)
    void latch();

    // Build a state snapshot from current pipeline register state
    PipelineState build_state(
        const StageSnapshot snaps[5],
        bool stalled, bool flushed) const;

    // Create a bubble IDEXReg (all control signals off)
    static IDEXReg make_bubble_idex();
    static IFIDReg make_bubble_ifid();

    void init_predictor(const std::string& type);
    void init_caches(const std::string& cfg_json);
};

} // namespace rv32i
