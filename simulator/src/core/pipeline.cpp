// =============================================================================
// pipeline.cpp — 5-Stage RISC-V Pipeline Implementation
// =============================================================================
//
// Execution order within one clock cycle:
//   1. WB  — write results back to register file
//   2. MEM — access data memory
//   3. EX  — ALU execution (with forwarding)
//   4. ID  — decode + register read
//   5. IF  — instruction fetch
//
// We process stages in WB→IF order so that we read pipeline registers BEFORE
// writing them. At the end of cycle we latch() all "next" values into "current".
// =============================================================================
#include "core/pipeline.h"
#include "branch/static_pred.h"
#include "branch/two_bit_pred.h"
#include "branch/gshare_pred.h"
#include "branch/tournament_pred.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>

namespace rv32i {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string hex32(uint32_t v) {
    char buf[12];
    snprintf(buf, sizeof(buf), "0x%08X", v);
    return buf;
}

Pipeline::Pipeline(PipelineConfig cfg) : config_(cfg) {
    init_predictor(cfg.predictor_type);
    init_caches(cfg.cache_config);
    reset();
}

void Pipeline::init_predictor(const std::string& type) {
    if (type == "static_taken")
        predictor_ = std::make_unique<StaticTakenPredictor>();
    else if (type == "two_bit")
        predictor_ = std::make_unique<TwoBitPredictor>(256);
    else if (type == "gshare")
        predictor_ = std::make_unique<GSharePredictor>(8);
    else if (type == "tournament")
        predictor_ = std::make_unique<TournamentPredictor>(256, 8);
    else // default: static_not_taken
        predictor_ = std::make_unique<StaticNotTakenPredictor>();
}

void Pipeline::init_caches(const std::string& /*cfg_json*/) {
    // I-Cache: 8KB, 4-way, 64-byte lines, LRU
    CacheConfig icfg;
    icfg.size_bytes = 8 * 1024; icfg.block_size = 64;
    icfg.associativity = 4; icfg.policy = ReplacementPolicy::LRU;
    icfg.name = "I-Cache";
    icache_ = std::make_unique<Cache>(icfg);

    // D-Cache: 8KB, 4-way, 64-byte lines, LRU
    CacheConfig dcfg;
    dcfg.size_bytes = 8 * 1024; dcfg.block_size = 64;
    dcfg.associativity = 4; dcfg.policy = ReplacementPolicy::LRU;
    dcfg.name = "D-Cache";
    dcache_ = std::make_unique<Cache>(dcfg);
}

void Pipeline::reset() {
    pc_     = TEXT_BASE;
    cycle_  = 0;
    halted_ = false;
    if_id_  = {}; id_ex_  = {}; ex_mem_ = {}; mem_wb_ = {};
    if_id_next_ = {}; id_ex_next_ = {}; ex_mem_next_ = {}; mem_wb_next_ = {};
    regs_.reset();
    regs_.write(2, (int32_t)STACK_BASE); // Initialize SP (x2) to stack base
    stats_.reset();
    icache_->reset();
    dcache_->reset();
    predictor_->reset();
}

bool Pipeline::load_program(const std::string& hex_str) {
    reset();
    return mem_.load_hex(hex_str);
}

IDEXReg Pipeline::make_bubble_idex() {
    IDEXReg b;
    b.valid   = false;
    b.asm_str = "--- (bubble)";
    return b;
}

IFIDReg Pipeline::make_bubble_ifid() {
    IFIDReg b;
    b.instruction = 0x00000013; // NOP
    b.valid       = false;
    b.asm_str     = "--- (bubble)";
    return b;
}

// ---------------------------------------------------------------------------
// ALU — takes two 32-bit operands and an operation, returns 32-bit result
// ---------------------------------------------------------------------------
int32_t Pipeline::alu_execute(ALUOp op, int32_t a, int32_t b) const {
    uint32_t ua = (uint32_t)a, ub = (uint32_t)b;
    switch (op) {
        case ALUOp::ADD:    return a + b;
        case ALUOp::SUB:    return a - b;
        case ALUOp::SLL:    return (int32_t)(ua << (ub & 31));
        case ALUOp::SLT:    return (a < b) ? 1 : 0;
        case ALUOp::SLTU:   return (ua < ub) ? 1 : 0;
        case ALUOp::XOR:    return a ^ b;
        case ALUOp::SRL:    return (int32_t)(ua >> (ub & 31));
        case ALUOp::SRA:    return a >> (ub & 31);  // arithmetic
        case ALUOp::OR:     return a | b;
        case ALUOp::AND:    return a & b;
        case ALUOp::PASS_B: return b;
        case ALUOp::PASS_A: return a;
        default:            return 0;
    }
}

// Branch condition evaluation
bool Pipeline::eval_branch(uint8_t funct3, int32_t alu_result, int32_t rs1, int32_t rs2) const {
    uint32_t u1 = (uint32_t)rs1, u2 = (uint32_t)rs2;
    switch (funct3) {
        case 0x0: return rs1 == rs2;       // BEQ
        case 0x1: return rs1 != rs2;       // BNE
        case 0x4: return rs1  <  rs2;      // BLT
        case 0x5: return rs1  >= rs2;      // BGE
        case 0x6: return u1   <  u2;       // BLTU
        case 0x7: return u1   >= u2;       // BGEU
        default:  return false;
    }
}

// ---------------------------------------------------------------------------
// WB Stage — Write results back to register file
// ---------------------------------------------------------------------------
void Pipeline::stage_wb(StageSnapshot& snap) {
    snap.stage = "WB";
    if (!mem_wb_.valid) {
        snap.bubble = true; snap.asm_str = "--- (bubble)"; return;
    }
    snap.asm_str = mem_wb_.asm_str;
    snap.pc      = mem_wb_.pc;
    snap.pc_hex  = hex32(mem_wb_.pc);

    if (mem_wb_.ctrl.reg_write && mem_wb_.rd != 0) {
        // WB MUX: select memory data or ALU result
        int32_t wb_data = mem_wb_.ctrl.mem_to_reg ? mem_wb_.mem_data : mem_wb_.alu_result;
        regs_.write(mem_wb_.rd, wb_data);
        snap.wb_rd     = mem_wb_.rd;
        snap.wb_value  = wb_data;
        snap.wb_active = true;
        ++stats_.instructions;
    } else if (mem_wb_.valid) {
        ++stats_.instructions; // branches, stores — they still retire
    }
}

// ---------------------------------------------------------------------------
// MEM Stage — Data memory access
// ---------------------------------------------------------------------------
void Pipeline::stage_mem(StageSnapshot& snap) {
    snap.stage = "MEM";
    // Propagate to MEM/WB
    mem_wb_next_.pc         = ex_mem_.pc;
    mem_wb_next_.alu_result = ex_mem_.alu_result;
    mem_wb_next_.rd         = ex_mem_.rd;
    mem_wb_next_.ctrl       = ex_mem_.ctrl;
    mem_wb_next_.valid      = ex_mem_.valid;
    mem_wb_next_.asm_str    = ex_mem_.asm_str;

    if (!ex_mem_.valid) {
        snap.bubble = true; snap.asm_str = "--- (bubble)"; return;
    }
    snap.asm_str = ex_mem_.asm_str;
    snap.pc      = ex_mem_.pc;
    snap.pc_hex  = hex32(ex_mem_.pc);

    uint32_t addr = (uint32_t)ex_mem_.alu_result;

    if (ex_mem_.ctrl.mem_read) {
        // Data cache access (load)
        bool hit = dcache_->access(addr, false);
        ++stats_.dcache_accesses; ++stats_.cache_accesses;
        if (hit) { ++stats_.dcache_hits; ++stats_.cache_hits; }
        else       ++stats_.cache_misses;

        snap.is_mem_op = true; snap.is_write = false; snap.cache_hit = hit;

        int32_t loaded = mem_.load(addr, ex_mem_.ctrl.mem_size, ex_mem_.ctrl.mem_signed);
        mem_wb_next_.mem_data = loaded;
    }
    else if (ex_mem_.ctrl.mem_write) {
        // Data cache access (store)
        bool hit = dcache_->access(addr, true);
        ++stats_.dcache_accesses; ++stats_.cache_accesses;
        if (hit) { ++stats_.dcache_hits; ++stats_.cache_hits; }
        else       ++stats_.cache_misses;

        snap.is_mem_op = true; snap.is_write = true; snap.cache_hit = hit;

        mem_.store(addr, ex_mem_.rs2_data, ex_mem_.ctrl.mem_size);
    }

    if (ex_mem_.ctrl.is_ecall) {
        halted_ = true;
    }
}

// ---------------------------------------------------------------------------
// EX Stage — ALU execution with data forwarding
// ---------------------------------------------------------------------------
void Pipeline::stage_ex(StageSnapshot& snap, const ForwardingSignals& fwd) {
    snap.stage = "EX";
    ex_mem_next_.pc      = id_ex_.pc;
    ex_mem_next_.pc_plus4= id_ex_.pc_plus4;
    ex_mem_next_.rd      = id_ex_.rd;
    ex_mem_next_.ctrl    = id_ex_.ctrl;
    ex_mem_next_.valid   = id_ex_.valid;
    ex_mem_next_.asm_str = id_ex_.asm_str;

    if (!id_ex_.valid) {
        snap.bubble = true; snap.asm_str = "--- (bubble)"; return;
    }
    snap.asm_str = id_ex_.asm_str;
    snap.pc      = id_ex_.pc;
    snap.pc_hex  = hex32(id_ex_.pc);

    // Resolve forwarded operand A (rs1)
    int32_t op_a = id_ex_.rs1_data;
    if (config_.forwarding_enabled) {
        if (fwd.forward_a == ForwardSel::EX_MEM) {
            op_a = ex_mem_.alu_result;
            snap.forward_a = "EX_MEM";
        } else if (fwd.forward_a == ForwardSel::MEM_WB) {
            op_a = mem_wb_.ctrl.mem_to_reg ? mem_wb_.mem_data : mem_wb_.alu_result;
            snap.forward_a = "MEM_WB";
        } else {
            snap.forward_a = "NONE";
        }
    }

    // Resolve forwarded operand B (rs2)
    int32_t op_b_reg = id_ex_.rs2_data;
    if (config_.forwarding_enabled) {
        if (fwd.forward_b == ForwardSel::EX_MEM) {
            op_b_reg = ex_mem_.alu_result;
            snap.forward_b = "EX_MEM";
        } else if (fwd.forward_b == ForwardSel::MEM_WB) {
            op_b_reg = mem_wb_.ctrl.mem_to_reg ? mem_wb_.mem_data : mem_wb_.alu_result;
            snap.forward_b = "MEM_WB";
        } else {
            snap.forward_b = "NONE";
        }
    }

    // ALU source MUX: immediate or register?
    int32_t alu_b = id_ex_.ctrl.alu_src ? id_ex_.imm : op_b_reg;

    // Special cases: AUIPC uses PC as operand A
    int32_t alu_a = id_ex_.ctrl.auipc ? (int32_t)id_ex_.pc : op_a;

    // Execute ALU
    int32_t alu_result = alu_execute(id_ex_.ctrl.alu_op, alu_a, alu_b);

    // Branch/Jump target calculation
    uint32_t branch_target = id_ex_.pc + (uint32_t)id_ex_.imm;
    bool branch_taken = false;

    if (id_ex_.ctrl.branch) {
        // Evaluate branch condition with actual (possibly forwarded) rs1, rs2
        branch_taken = eval_branch(
            // We stored funct3 implicitly in alu_op; use actual register values
            [&]() -> uint8_t {
                // Recover funct3 from the alu_op to pick evaluation
                switch (id_ex_.ctrl.alu_op) {
                    case ALUOp::SUB:  {
                        // BEQ or BNE — we need to check if equal
                        // Use op_a and op_b_reg directly for comparison
                        return 0xFF; // handled below
                    }
                    default: return 0xFF;
                }
            }(),
            alu_result, op_a, op_b_reg
        );

        // Simpler: directly evaluate using op_a, op_b_reg and mnemonic parsing
        // We encode funct3 in asm_str implicitly; instead, let's re-decode from raw
        // Actually, we don't have funct3 here. Let's store it in IDEXReg.
        // For now, use the alu_result to determine branch:
        // BEQ/BNE: alu_op=SUB, zero flag
        // BLT/BGE: alu_op=SLT, result is 0 or 1
        // BLTU/BGEU: alu_op=SLTU
        //
        // This is a known limitation of our current IDEXReg — fix by storing funct3.
        // For the code to work, we use op_a vs op_b_reg directly:
        bool eq = (op_a == op_b_reg);
        bool lt = (op_a <  op_b_reg); // signed
        uint32_t ua = (uint32_t)op_a, ub = (uint32_t)op_b_reg;
        bool ltu = (ua < ub);

        // Re-determine which comparison based on ALUOp
        switch (id_ex_.ctrl.alu_op) {
            case ALUOp::SUB:  // BEQ (eq) or BNE (!eq)
                // We can't know which without funct3. Store funct3 in IDEXReg.
                // Workaround: check asm_str prefix
                if (id_ex_.asm_str.substr(0,3) == "BEQ")  branch_taken = eq;
                else if (id_ex_.asm_str.substr(0,3) == "BNE")  branch_taken = !eq;
                else if (id_ex_.asm_str.substr(0,3) == "BGE")  branch_taken = !lt;
                break;
            case ALUOp::SLT:  // BLT or BGE
                if (id_ex_.asm_str.substr(0,3) == "BLT")  branch_taken = lt;
                else                                        branch_taken = !lt;
                break;
            case ALUOp::SLTU: // BLTU or BGEU
                if (id_ex_.asm_str.substr(0,4) == "BLTU") branch_taken = ltu;
                else                                        branch_taken = !ltu;
                break;
            default: break;
        }

        if (branch_taken) {
            ++stats_.branch_count;
            // Update predictor
            predictor_->update(id_ex_.pc, branch_taken);
        } else {
            ++stats_.branch_count;
            predictor_->update(id_ex_.pc, false);
        }
    }

    if (id_ex_.ctrl.jump) { // JAL: rd = PC+4, jump to PC+imm
        branch_taken  = true;
        branch_target = id_ex_.pc + (uint32_t)id_ex_.imm;
        alu_result    = (int32_t)id_ex_.pc_plus4; // Return address
    }

    if (id_ex_.ctrl.jalr) { // JALR: rd = PC+4, jump to rs1+imm & ~1
        branch_taken  = true;
        branch_target = ((uint32_t)(op_a + id_ex_.imm)) & ~1u;
        alu_result    = (int32_t)id_ex_.pc_plus4;
    }

    ex_mem_next_.alu_result   = alu_result;
    ex_mem_next_.rs2_data     = op_b_reg;
    ex_mem_next_.branch_taken  = branch_taken;
    ex_mem_next_.branch_target = branch_target;
}

// ---------------------------------------------------------------------------
// ID Stage — Instruction decode, register read
// ---------------------------------------------------------------------------
void Pipeline::stage_id(StageSnapshot& snap, const HazardSignals& hz) {
    snap.stage = "ID";

    if (hz.flush_ex || !if_id_.valid) {
        // Insert bubble
        id_ex_next_  = make_bubble_idex();
        snap.bubble  = true;
        snap.asm_str = "--- (bubble)";
        return;
    }

    if (hz.stall_id) {
        // Freeze: re-latch same IF/ID (handled by not writing id_ex_next_ here)
        // id_ex_next_ is set to bubble already by hazard stall
        id_ex_next_  = make_bubble_idex();
        snap.stalled = true;
        snap.asm_str = if_id_.asm_str;
        snap.pc      = if_id_.pc;
        snap.pc_hex  = hex32(if_id_.pc);
        return;
    }

    DecodedInstr d = Decoder::decode(if_id_.instruction, if_id_.pc);
    snap.asm_str = d.mnemonic;
    snap.pc      = if_id_.pc;
    snap.pc_hex  = hex32(if_id_.pc);

    id_ex_next_.pc       = if_id_.pc;
    id_ex_next_.pc_plus4 = if_id_.pc_plus4;
    id_ex_next_.rs1      = d.rs1;
    id_ex_next_.rs2      = d.rs2;
    id_ex_next_.rd       = d.rd;
    id_ex_next_.imm      = d.imm;
    id_ex_next_.ctrl     = d.ctrl;
    id_ex_next_.asm_str  = d.mnemonic;
    id_ex_next_.valid    = true;

    // Read register file
    id_ex_next_.rs1_data = regs_.read(d.rs1);
    id_ex_next_.rs2_data = regs_.read(d.rs2);
}

// ---------------------------------------------------------------------------
// IF Stage — Instruction fetch
// ---------------------------------------------------------------------------
void Pipeline::stage_if(StageSnapshot& snap, const HazardSignals& hz) {
    snap.stage = "IF";

    if (hz.flush_if) {
        // Flush: replace with bubble
        if_id_next_  = make_bubble_ifid();
        snap.bubble  = true;
        snap.asm_str = "--- (flushed)";
        return;
    }

    if (hz.stall_if) {
        // Stall: keep same IF/ID, don't advance PC
        if_id_next_ = if_id_; // Keep current
        snap.stalled = true;
        snap.asm_str = if_id_.asm_str;
        snap.pc      = pc_;
        snap.pc_hex  = hex32(pc_);
        return;
    }

    if (halted_) {
        if_id_next_ = make_bubble_ifid();
        snap.bubble  = true;
        snap.asm_str = "--- (halted)";
        return;
    }

    // I-Cache access for instruction fetch
    bool ihit = icache_->access(pc_, false);
    ++stats_.icache_accesses; ++stats_.cache_accesses;
    if (ihit) ++stats_.icache_hits;
    else       ++stats_.cache_misses;

    uint32_t raw = mem_.fetch32(pc_);
    DecodedInstr d = Decoder::decode(raw, pc_);

    if_id_next_.instruction = raw;
    if_id_next_.pc          = pc_;
    if_id_next_.pc_plus4    = pc_ + 4;
    if_id_next_.valid       = true;
    if_id_next_.asm_str     = d.mnemonic;

    snap.asm_str = d.mnemonic;
    snap.pc      = pc_;
    snap.pc_hex  = hex32(pc_);

    pc_ += 4;
}

// ---------------------------------------------------------------------------
// latch() — promote "next" pipeline registers to "current"
// ---------------------------------------------------------------------------
void Pipeline::latch() {
    if_id_  = if_id_next_;
    id_ex_  = id_ex_next_;
    ex_mem_ = ex_mem_next_;
    mem_wb_ = mem_wb_next_;
}

// ---------------------------------------------------------------------------
// step() — one clock cycle
// ---------------------------------------------------------------------------
PipelineState Pipeline::step() {
    if (halted_) return build_state(nullptr, false, false);

    ++cycle_;
    ++stats_.cycles;

    StageSnapshot snaps[5];

    // --- Phase 1: determine branch outcome (from current EX/MEM) ---
    // We peek at the EX stage output from last cycle (ex_mem_ before update)
    // Actually, the branch is resolved DURING EX. We need to compute it first.
    // We'll run EX first to get branch_taken, then detect hazards.

    // Pre-compute forwarding for EX stage
    ForwardingSignals fwd = config_.forwarding_enabled
        ? ForwardingUnit::compute(id_ex_, ex_mem_, mem_wb_)
        : ForwardingSignals{};

    // Pre-compute EX result to know if branch was taken
    // (We run EX in a "speculative" pass first to determine branch_taken)
    bool branch_taken_this_cycle = false;
    if (id_ex_.valid && (id_ex_.ctrl.branch || id_ex_.ctrl.jump || id_ex_.ctrl.jalr)) {
        // Simplified branch check: we'll compute properly in stage_ex
        // For hazard detection, we need to know NOW
        // We detect it properly — stage_ex writes to ex_mem_next_.branch_taken
    }

    // Detect hazards
    HazardSignals hz = config_.hazard_detection
        ? HazardUnit::detect(id_ex_, if_id_, false) // branch_taken resolved later
        : HazardSignals{};

    // Run stages WB → IF
    stage_wb (snaps[4]);
    stage_mem(snaps[3]);
    stage_ex (snaps[2], fwd);
    stage_id (snaps[1], hz);
    stage_if (snaps[0], hz);

    // Check if branch was taken (set during stage_ex)
    branch_taken_this_cycle = ex_mem_next_.branch_taken;

    // Apply control hazard flush if branch was taken
    if (branch_taken_this_cycle && !hz.load_use) {
        // Flush IF and ID (instructions fetched speculatively)
        if_id_next_ = make_bubble_ifid();
        id_ex_next_ = make_bubble_idex();
        // Update PC to branch target
        pc_ = ex_mem_next_.branch_target;
        ++stats_.flush_cycles;
        ++stats_.flush_cycles; // 2 cycles penalty
    }

    if (hz.load_use) {
        ++stats_.stall_cycles;
    }

    // Track mispredictions (when branch actually taken but predictor said not)
    // For now, since we predict not-taken by default, every taken branch = mispred
    // The predictor module tracks its own stats

    // Latch all pipeline registers
    latch();

    bool stalled = hz.load_use;
    bool flushed = branch_taken_this_cycle && !hz.load_use;

    last_state_ = build_state(snaps, stalled, flushed);
    return last_state_;
}

// ---------------------------------------------------------------------------
// run() — run until halted or max_cycles reached
// ---------------------------------------------------------------------------
PipelineState Pipeline::run(int max_cycles) {
    PipelineState s;
    for (int i = 0; i < max_cycles && !halted_; ++i) {
        s = step();
    }
    return s;
}

// ---------------------------------------------------------------------------
// build_state() — assemble the full JSON-serializable state snapshot
// ---------------------------------------------------------------------------
PipelineState Pipeline::build_state(const StageSnapshot snaps[5], bool stalled, bool flushed) const {
    PipelineState s;
    s.cycle   = cycle_;
    s.halted  = halted_;
    s.stall_this_cycle  = stalled;
    s.flush_this_cycle  = flushed;
    s.stats   = stats_;
    s.registers   = regs_.to_json();
    s.cache_state = {
        {"icache", icache_->get_state()},
        {"dcache", dcache_->get_state()}
    };
    s.predictor_state = predictor_->get_state();

    if (snaps) {
        for (int i = 0; i < 5; ++i)
            s.stages[i] = snaps[i];
    }
    return s;
}

PipelineState Pipeline::get_state() const {
    return last_state_;
}

void Pipeline::configure(const PipelineConfig& cfg) {
    config_ = cfg;
    init_predictor(cfg.predictor_type);
    init_caches(cfg.cache_config);
    // Don't reset — allow hot-swap
}

} // namespace rv32i
