// =============================================================================
// superscalar_pipeline.h — 2-Wide Superscalar RISC-V Pipeline
// =============================================================================
//
// Models a 2-wide superscalar processor with three dispatch/completion policies:
//
//   1. IO/IO   — In-Order issue, In-Order completion
//   2. OoO/IO  — Out-of-Order issue, In-Order completion (with ROB)
//   3. OoO/OoO — Out-of-Order issue, Out-of-Order completion (Tomasulo)
//
// Educational focus: show how width and scheduling policy affect IPC.
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
#include <deque>
#include <string>

namespace rv32i {

// ---------------------------------------------------------------------------
// Superscalar scheduling policy
// ---------------------------------------------------------------------------
enum class SuperscalarPolicy {
    IO_IO,     // In-order issue, in-order completion
    OOO_IO,    // Out-of-order issue, in-order completion
    OOO_OOO    // Out-of-order issue, out-of-order completion
};

// ---------------------------------------------------------------------------
// Functional unit types (2-wide: ALU0+ALU1, plus MEM and BRANCH)
// ---------------------------------------------------------------------------
enum class FUType { ALU0, ALU1, MEM, BRANCH, NONE };

// ---------------------------------------------------------------------------
// Reorder Buffer entry
// ---------------------------------------------------------------------------
struct ROBEntry {
    int         id          = -1;
    uint32_t    pc          = 0;
    std::string asm_str;
    uint8_t     rd          = 0;
    int32_t     result      = 0;
    bool        ready       = false;   // result has been computed
    bool        committed   = false;   // written to arch register file
    bool        is_branch   = false;
    bool        branch_taken= false;
    uint32_t    branch_target= 0;
    bool        is_store    = false;
    bool        is_load     = false;
    bool        is_ecall    = false;
    FUType      fu_used     = FUType::NONE;
    bool        valid       = false;
};

// ---------------------------------------------------------------------------
// Reservation Station entry (for OoO modes)
// ---------------------------------------------------------------------------
struct RSEntry {
    int         rob_id  = -1;
    ALUOp       op      = ALUOp::NOP;
    int32_t     vj = 0, vk = 0;      // operand values (when ready)
    int         qj = -1, qk = -1;    // ROB tags (-1 = value is ready)
    uint8_t     rd = 0;
    uint8_t     rs1 = 0, rs2 = 0;
    int32_t     imm = 0;
    ControlSignals ctrl;
    bool        busy    = false;
    FUType      fu_type = FUType::NONE;
    uint32_t    pc      = 0;
    std::string asm_str;
};

// ---------------------------------------------------------------------------
// Instruction queue entry (fetched + decoded, waiting to issue)
// ---------------------------------------------------------------------------
struct IQEntry {
    DecodedInstr instr;
    uint32_t     pc = 0;
    std::string  asm_str;
    bool         valid = false;
};

// ---------------------------------------------------------------------------
// SuperscalarPipeline
// ---------------------------------------------------------------------------
class SuperscalarPipeline : public SimulatorEngine {
public:
    explicit SuperscalarPipeline(SuperscalarPolicy policy = SuperscalarPolicy::IO_IO);

    // SimulatorEngine interface
    nlohmann::json step() override;
    nlohmann::json run(int max_cycles = 100000) override;
    bool load_program(const std::string& hex_str) override;
    void reset() override;
    nlohmann::json get_state_json() const override;
    void configure(const nlohmann::json& cfg) override;
    bool is_halted() const override { return halted_; }
    uint64_t cycle_count() const override { return cycle_; }
    std::string engine_type() const override { return "superscalar"; }

    void set_policy(SuperscalarPolicy p) { policy_ = p; reset(); }

private:
    SuperscalarPolicy policy_;

    // Processor state
    uint32_t    pc_;
    uint64_t    cycle_;
    bool        halted_;

    // Submodules
    RegisterFile        regs_;           // Architectural register file
    Memory              mem_;
    Decoder             decoder_;
    std::unique_ptr<Predictor> predictor_;
    std::unique_ptr<Cache>     icache_;
    std::unique_ptr<Cache>     dcache_;
    PerfCounters        stats_;

    // Register rename table (maps arch reg → ROB id, or -1 if in arch file)
    int reg_rename_[32];

    // Reorder Buffer (circular)
    static constexpr int ROB_SIZE = 16;
    ROBEntry rob_[ROB_SIZE];
    int rob_head_, rob_tail_;
    int rob_count_;

    // Reservation stations
    static constexpr int RS_COUNT = 8;
    RSEntry rs_[RS_COUNT];

    // Instruction queue (fetched, decoded, waiting for issue)
    std::deque<IQEntry> instr_queue_;
    static constexpr int IQ_MAX = 8;

    // Functional unit busy counters (0 = available)
    int fu_busy_[4];  // ALU0, ALU1, MEM, BRANCH
    struct FUResult { int rob_id; int32_t value; FUType fu; bool valid; };
    FUResult fu_results_[4];

    // Per-cycle visualization
    struct CycleViz {
        int fetched = 0;
        int issued  = 0;
        int completed = 0;
        int committed = 0;
        std::string fetch_asm[2];
        std::string issue_asm[2];
        bool stall  = false;
        bool flush  = false;
    } cycle_viz_;

    // -----------------------------------------------------------------------
    // Pipeline phases (execute in reverse order each cycle)
    // -----------------------------------------------------------------------
    void phase_commit();
    void phase_writeback();
    void phase_execute();
    void phase_issue();
    void phase_fetch_decode();

    // IO/IO specific: simplified in-order issue
    void phase_issue_inorder();

    // Helpers
    int  rob_alloc();
    void rob_free_head();
    bool rob_full() const { return rob_count_ >= ROB_SIZE; }
    int  rs_alloc(FUType fu);
    int32_t execute_alu(ALUOp op, int32_t a, int32_t b) const;
    bool eval_branch(uint8_t funct3, int32_t rs1, int32_t rs2) const;
    FUType classify_instruction(const DecodedInstr& instr) const;
    bool has_data_dependency(const IQEntry& instr) const;
    int32_t read_operand(uint8_t reg_idx) const;
    nlohmann::json build_state_json() const;

    void init_predictor(const std::string& type);
    void init_caches();
};

} // namespace rv32i
