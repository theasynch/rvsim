// =============================================================================
// vliw_pipeline.h — VLIW (Very Long Instruction Word) RISC-V Pipeline
// =============================================================================
//
// Models a 3-slot VLIW processor where the "compiler" (our bundler) packs
// independent instructions into bundles:
//
//   Slot 0: ALU   (arithmetic/logic operations)
//   Slot 1: MEM   (load/store operations)
//   Slot 2: BRU   (branch/jump operations)
//
// Empty slots are filled with NOPs. The bundler tries to pack independent
// sequential instructions into the same bundle for maximum slot utilization.
//
// Educational focus: compiler-managed parallelism vs hardware scheduling.
//
// =============================================================================
#pragma once

#include "core/simulator_engine.h"
#include "core/register_file.h"
#include "core/memory.h"
#include "branch/predictor.h"
#include "cache/cache.h"
#include "stats/perf_counters.h"
#include "isa/decoder.h"
#include "isa/rv32i_types.h"
#include <memory>
#include <vector>
#include <string>

namespace rv32i {

// ---------------------------------------------------------------------------
// VLIW Bundle — one "very long instruction word" with 3 slots
// ---------------------------------------------------------------------------
struct VLIWSlot {
    DecodedInstr instr;
    uint32_t     pc       = 0;
    std::string  asm_str  = "NOP";
    bool         valid    = false;  // false = NOP slot
    bool         is_nop   = true;
};

struct VLIWBundle {
    VLIWSlot   slots[3];    // ALU, MEM, BRU
    uint32_t   bundle_pc = 0;
    bool       valid     = false;
    std::string to_string() const;
};

// ---------------------------------------------------------------------------
// VLIW pipeline register (between stages, holds entire bundle)
// ---------------------------------------------------------------------------
struct VLIWPipeReg {
    VLIWBundle bundle;
    bool       valid  = false;
    bool       bubble = false;
};

// ---------------------------------------------------------------------------
// VLIWPipeline
// ---------------------------------------------------------------------------
class VLIWPipeline : public SimulatorEngine {
public:
    explicit VLIWPipeline();

    // SimulatorEngine interface
    nlohmann::json step() override;
    nlohmann::json run(int max_cycles = 100000) override;
    bool load_program(const std::string& hex_str) override;
    void reset() override;
    nlohmann::json get_state_json() const override;
    void configure(const nlohmann::json& cfg) override;
    bool is_halted() const override { return halted_; }
    uint64_t cycle_count() const override { return cycle_; }
    std::string engine_type() const override { return "vliw"; }

private:
    // Processor state
    uint32_t    pc_;
    uint64_t    cycle_;
    bool        halted_;

    // Submodules
    RegisterFile        regs_;
    Memory              mem_;
    Decoder             decoder_;
    std::unique_ptr<Predictor> predictor_;
    std::unique_ptr<Cache>     icache_;
    std::unique_ptr<Cache>     dcache_;
    PerfCounters        stats_;

    // VLIW-specific stats
    int total_slots_;    // cycle * 3
    int used_slots_;     // non-NOP slots executed

    // Bundled program (created at load time)
    std::vector<VLIWBundle> program_bundles_;
    int bundle_idx_;  // index into program_bundles_ (like PC for bundles)

    // Pipeline registers (5-stage: IF, ID, EX, MEM, WB — each holds a bundle)
    VLIWPipeReg pipe_[5]; // 0=IF, 1=ID, 2=EX, 3=MEM, 4=WB

    // Per-cycle visualization
    struct CycleViz {
        bool stall = false;
        bool flush = false;
    } cycle_viz_;

    // -----------------------------------------------------------------------
    // Pipeline stages
    // -----------------------------------------------------------------------
    void stage_wb();
    void stage_mem();
    void stage_ex();
    void stage_id();
    void stage_if();

    // -----------------------------------------------------------------------
    // Bundler — pack sequential instructions into VLIW bundles
    // -----------------------------------------------------------------------
    void bundle_program();
    int classify_slot(const DecodedInstr& instr) const;
    bool has_dependency(const DecodedInstr& a, const DecodedInstr& b) const;

    // Helpers
    int32_t execute_alu(ALUOp op, int32_t a, int32_t b) const;
    bool eval_branch(uint8_t funct3, int32_t rs1, int32_t rs2) const;
    nlohmann::json build_state_json() const;
    void init_predictor(const std::string& type);
    void init_caches();
};

} // namespace rv32i
