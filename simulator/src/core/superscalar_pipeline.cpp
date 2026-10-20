// =============================================================================
// superscalar_pipeline.cpp — 2-Wide Superscalar RISC-V Pipeline
// =============================================================================
#include "core/superscalar_pipeline.h"
#include "branch/static_pred.h"
#include "branch/two_bit_pred.h"
#include "branch/gshare_pred.h"
#include "branch/tournament_pred.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace rv32i {

static std::string hex32(uint32_t v) {
    char buf[12];
    snprintf(buf, sizeof(buf), "0x%08X", v);
    return buf;
}

// ---------------------------------------------------------------------------
// Construction / Init
// ---------------------------------------------------------------------------
SuperscalarPipeline::SuperscalarPipeline(SuperscalarPolicy policy)
    : policy_(policy)
{
    init_predictor("static_not_taken");
    init_caches();
    reset();
}

void SuperscalarPipeline::init_predictor(const std::string& type) {
    if (type == "static_taken")       predictor_ = std::make_unique<StaticTakenPredictor>();
    else if (type == "two_bit")       predictor_ = std::make_unique<TwoBitPredictor>(256);
    else if (type == "gshare")        predictor_ = std::make_unique<GSharePredictor>(8);
    else if (type == "tournament")    predictor_ = std::make_unique<TournamentPredictor>(256, 8);
    else                              predictor_ = std::make_unique<StaticNotTakenPredictor>();
}

void SuperscalarPipeline::init_caches() {
    CacheConfig icfg;
    icfg.size_bytes = 8*1024; icfg.block_size = 64;
    icfg.associativity = 4; icfg.policy = ReplacementPolicy::LRU;
    icfg.name = "I-Cache";
    icache_ = std::make_unique<Cache>(icfg);

    CacheConfig dcfg;
    dcfg.size_bytes = 8*1024; dcfg.block_size = 64;
    dcfg.associativity = 4; dcfg.policy = ReplacementPolicy::LRU;
    dcfg.name = "D-Cache";
    dcache_ = std::make_unique<Cache>(dcfg);
}

void SuperscalarPipeline::reset() {
    pc_ = 0x00000000;
    cycle_ = 0;
    halted_ = false;
    regs_.reset();
    regs_.write(2, (int32_t)0x7FFFFFF0); // SP
    stats_.reset();
    icache_->reset();
    dcache_->reset();
    predictor_->reset();
    instr_queue_.clear();
    memset(reg_rename_, -1, sizeof(reg_rename_));
    rob_head_ = rob_tail_ = rob_count_ = 0;
    for (int i = 0; i < ROB_SIZE; ++i) rob_[i] = {};
    for (int i = 0; i < RS_COUNT; ++i) rs_[i] = {};
    for (int i = 0; i < 4; ++i) { fu_busy_[i] = 0; fu_results_[i] = {}; }
    cycle_viz_ = {};
}

bool SuperscalarPipeline::load_program(const std::string& hex_str) {
    reset();
    return mem_.load_hex(hex_str);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
int SuperscalarPipeline::rob_alloc() {
    if (rob_count_ >= ROB_SIZE) return -1;
    int idx = rob_tail_;
    rob_[idx] = {};
    rob_[idx].id = idx;
    rob_[idx].valid = true;
    rob_tail_ = (rob_tail_ + 1) % ROB_SIZE;
    rob_count_++;
    return idx;
}

void SuperscalarPipeline::rob_free_head() {
    if (rob_count_ <= 0) return;
    rob_[rob_head_] = {};
    rob_head_ = (rob_head_ + 1) % ROB_SIZE;
    rob_count_--;
}

int SuperscalarPipeline::rs_alloc(FUType fu) {
    for (int i = 0; i < RS_COUNT; ++i) {
        if (!rs_[i].busy) return i;
    }
    return -1;
}

FUType SuperscalarPipeline::classify_instruction(const DecodedInstr& instr) const {
    if (instr.ctrl.branch || instr.ctrl.jump || instr.ctrl.jalr) return FUType::BRANCH;
    if (instr.ctrl.mem_read || instr.ctrl.mem_write) return FUType::MEM;
    return FUType::ALU0; // Will be assigned ALU0 or ALU1 at issue time
}

int32_t SuperscalarPipeline::execute_alu(ALUOp op, int32_t a, int32_t b) const {
    uint32_t ua = (uint32_t)a, ub = (uint32_t)b;
    switch (op) {
        case ALUOp::ADD:    return a + b;
        case ALUOp::SUB:    return a - b;
        case ALUOp::SLL:    return (int32_t)(ua << (ub & 31));
        case ALUOp::SLT:    return (a < b) ? 1 : 0;
        case ALUOp::SLTU:   return (ua < ub) ? 1 : 0;
        case ALUOp::XOR:    return a ^ b;
        case ALUOp::SRL:    return (int32_t)(ua >> (ub & 31));
        case ALUOp::SRA:    return a >> (ub & 31);
        case ALUOp::OR:     return a | b;
        case ALUOp::AND:    return a & b;
        case ALUOp::PASS_B: return b;
        case ALUOp::PASS_A: return a;
        default:            return 0;
    }
}

bool SuperscalarPipeline::eval_branch(uint8_t funct3, int32_t rs1, int32_t rs2) const {
    uint32_t u1 = (uint32_t)rs1, u2 = (uint32_t)rs2;
    switch (funct3) {
        case 0: return rs1 == rs2;  // BEQ
        case 1: return rs1 != rs2;  // BNE
        case 4: return rs1 < rs2;   // BLT
        case 5: return rs1 >= rs2;  // BGE
        case 6: return u1 < u2;     // BLTU
        case 7: return u1 >= u2;    // BGEU
        default: return false;
    }
}

int32_t SuperscalarPipeline::read_operand(uint8_t reg_idx) const {
    if (reg_idx == 0) return 0;
    // In OoO modes, check rename table → ROB for value
    if (policy_ != SuperscalarPolicy::IO_IO && reg_rename_[reg_idx] >= 0) {
        int rob_id = reg_rename_[reg_idx];
        if (rob_[rob_id].valid && rob_[rob_id].ready) {
            return rob_[rob_id].result;
        }
    }
    return regs_.read(reg_idx);
}

bool SuperscalarPipeline::has_data_dependency(const IQEntry& entry) const {
    const auto& instr = entry.instr;
    // Check if rs1 or rs2 are being written by an in-flight instruction
    if (policy_ == SuperscalarPolicy::IO_IO) {
        // In-order: check if any ROB entry targets rs1/rs2 and isn't ready
        for (int i = 0; i < ROB_SIZE; ++i) {
            if (!rob_[i].valid || rob_[i].committed) continue;
            if (!rob_[i].ready) {
                if (rob_[i].rd == instr.rs1 && instr.rs1 != 0) return true;
                if (rob_[i].rd == instr.rs2 && instr.rs2 != 0) return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// PHASE: Fetch + Decode (fetch up to 2 instructions per cycle)
// ---------------------------------------------------------------------------
void SuperscalarPipeline::phase_fetch_decode() {
    cycle_viz_.fetched = 0;
    if (halted_) return;

    for (int slot = 0; slot < 2 && (int)instr_queue_.size() < IQ_MAX; ++slot) {
        uint32_t word = mem_.load(pc_, MemSize::WORD, false);
        if (word == 0) { halted_ = true; break; }

        // I-Cache access
        icache_->access(pc_, false);

        DecodedInstr di = decoder_.decode(word, pc_);
        if (!di.valid) { halted_ = true; break; }

        IQEntry iq;
        iq.instr   = di;
        iq.pc      = pc_;
        iq.asm_str = di.mnemonic;
        iq.valid   = true;
        instr_queue_.push_back(iq);

        if (slot < 2) cycle_viz_.fetch_asm[slot] = di.mnemonic;
        cycle_viz_.fetched++;

        pc_ += 4;

        // Stop fetching after a branch/jump (we don't know the target yet)
        if (di.ctrl.branch || di.ctrl.jump || di.ctrl.jalr) break;
        if (di.ctrl.is_ecall) break;
    }
}

// ---------------------------------------------------------------------------
// PHASE: Issue — In-Order (IO/IO mode)
// ---------------------------------------------------------------------------
void SuperscalarPipeline::phase_issue_inorder() {
    cycle_viz_.issued = 0;
    int issued_this_cycle = 0;

    while (!instr_queue_.empty() && issued_this_cycle < 2) {
        auto& front = instr_queue_.front();
        if (!front.valid) { instr_queue_.pop_front(); continue; }

        // Check data dependencies
        if (has_data_dependency(front)) {
            cycle_viz_.stall = true;
            break; // In-order: can't skip past a blocked instruction
        }

        // Check ROB space
        if (rob_full()) break;

        // Classify instruction and check FU availability
        FUType fu = classify_instruction(front.instr);
        int fu_idx = (int)fu;
        if (fu == FUType::ALU0 && fu_busy_[0] > 0) {
            if (fu_busy_[1] > 0) break; // both ALUs busy
            fu = FUType::ALU1;
            fu_idx = 1;
        }
        if (fu_busy_[fu_idx] > 0) break;

        // Allocate ROB entry
        int rob_id = rob_alloc();
        if (rob_id < 0) break;

        ROBEntry& re = rob_[rob_id];
        re.pc       = front.pc;
        re.asm_str  = front.asm_str;
        re.rd       = front.instr.rd;
        re.is_branch= front.instr.ctrl.branch || front.instr.ctrl.jump || front.instr.ctrl.jalr;
        re.is_store = front.instr.ctrl.mem_write;
        re.is_load  = front.instr.ctrl.mem_read;
        re.is_ecall = front.instr.ctrl.is_ecall;
        re.fu_used  = fu;

        // Read operands directly (in-order, so all deps are resolved)
        int32_t op_a = regs_.read(front.instr.rs1);
        int32_t op_b = front.instr.ctrl.alu_src ? front.instr.imm : regs_.read(front.instr.rs2);

        // Execute immediately (1-cycle latency for ALU, 1 for MEM in our model)
        if (fu == FUType::ALU0 || fu == FUType::ALU1) {
            re.result = execute_alu(front.instr.ctrl.alu_op, op_a, op_b);
            if (front.instr.ctrl.lui) re.result = front.instr.imm;
            if (front.instr.ctrl.auipc) re.result = (int32_t)(front.pc + front.instr.imm);
            if (front.instr.ctrl.jump || front.instr.ctrl.jalr) re.result = (int32_t)(front.pc + 4);
            re.ready = true;
        } else if (fu == FUType::MEM) {
            int32_t addr = op_a + front.instr.imm;
            if (front.instr.ctrl.mem_read) {
                re.result = mem_.load((uint32_t)addr, MemSize::WORD, true);
                bool hit = dcache_->access((uint32_t)addr, false);
                if (!hit) stats_.cache_misses++;
                stats_.cache_accesses++;
            } else if (front.instr.ctrl.mem_write) {
                mem_.store((uint32_t)addr, regs_.read(front.instr.rs2), MemSize::WORD);
                dcache_->access((uint32_t)addr, true);
            }
            re.ready = true;
        } else if (fu == FUType::BRANCH) {
            if (front.instr.ctrl.is_ecall) {
                re.ready = true;
            } else {
                int32_t rs1_val = regs_.read(front.instr.rs1);
                int32_t rs2_val = regs_.read(front.instr.rs2);
                bool taken = false;
                uint32_t target = 0;
                if (front.instr.ctrl.branch) {
                    taken = eval_branch(front.instr.funct3, rs1_val, rs2_val);
                    target = front.pc + front.instr.imm;
                } else if (front.instr.ctrl.jump) {
                    taken = true;
                    target = front.pc + front.instr.imm;
                } else if (front.instr.ctrl.jalr) {
                    taken = true;
                    target = (uint32_t)(rs1_val + front.instr.imm) & ~1u;
                }
                re.branch_taken = taken;
                re.branch_target = target;
                re.result = (int32_t)(front.pc + 4); // link address
                re.ready = true;
            }
        }

        // Update rename table
        if (re.rd != 0 && front.instr.ctrl.reg_write) {
            reg_rename_[re.rd] = rob_id;
        }

        fu_busy_[fu_idx] = 1; // 1-cycle latency

        if (issued_this_cycle < 2) cycle_viz_.issue_asm[issued_this_cycle] = front.asm_str;
        issued_this_cycle++;
        instr_queue_.pop_front();
    }
    cycle_viz_.issued = issued_this_cycle;
    if (issued_this_cycle == 2) stats_.dual_issue_cycles++;
}

// ---------------------------------------------------------------------------
// PHASE: Issue — Out-of-Order (OoO/IO and OoO/OoO modes)
// ---------------------------------------------------------------------------
void SuperscalarPipeline::phase_issue() {
    if (policy_ == SuperscalarPolicy::IO_IO) {
        phase_issue_inorder();
        return;
    }

    cycle_viz_.issued = 0;
    int issued_this_cycle = 0;

    // OoO: scan instruction queue for instructions whose operands are ready
    auto it = instr_queue_.begin();
    while (it != instr_queue_.end() && issued_this_cycle < 2) {
        if (!it->valid) { it = instr_queue_.erase(it); continue; }

        FUType fu = classify_instruction(it->instr);

        // Check FU availability
        int fu_idx = (int)fu;
        if (fu == FUType::ALU0 && fu_busy_[0] > 0) {
            fu = FUType::ALU1; fu_idx = 1;
        }
        if (fu_busy_[fu_idx] > 0) { ++it; continue; }

        // Check ROB space
        if (rob_full()) break;

        // Allocate RS
        int rs_idx = rs_alloc(fu);
        if (rs_idx < 0) break;

        // Allocate ROB
        int rob_id = rob_alloc();
        if (rob_id < 0) break;

        ROBEntry& re = rob_[rob_id];
        re.pc       = it->pc;
        re.asm_str  = it->asm_str;
        re.rd       = it->instr.rd;
        re.is_branch= it->instr.ctrl.branch || it->instr.ctrl.jump || it->instr.ctrl.jalr;
        re.is_store = it->instr.ctrl.mem_write;
        re.is_load  = it->instr.ctrl.mem_read;
        re.is_ecall = it->instr.ctrl.is_ecall;
        re.fu_used  = fu;

        // Fill reservation station
        RSEntry& r = rs_[rs_idx];
        r.rob_id  = rob_id;
        r.op      = it->instr.ctrl.alu_op;
        r.rd      = it->instr.rd;
        r.rs1     = it->instr.rs1;
        r.rs2     = it->instr.rs2;
        r.imm     = it->instr.imm;
        r.ctrl    = it->instr.ctrl;
        r.pc      = it->pc;
        r.asm_str = it->asm_str;
        r.fu_type = fu;
        r.busy    = true;

        // Check operand availability via rename table
        if (it->instr.rs1 != 0 && reg_rename_[it->instr.rs1] >= 0) {
            int src_rob = reg_rename_[it->instr.rs1];
            if (rob_[src_rob].valid && rob_[src_rob].ready) {
                r.vj = rob_[src_rob].result; r.qj = -1;
            } else {
                r.qj = src_rob;
            }
        } else {
            r.vj = regs_.read(it->instr.rs1); r.qj = -1;
        }

        if (it->instr.rs2 != 0 && reg_rename_[it->instr.rs2] >= 0 && !it->instr.ctrl.alu_src) {
            int src_rob = reg_rename_[it->instr.rs2];
            if (rob_[src_rob].valid && rob_[src_rob].ready) {
                r.vk = rob_[src_rob].result; r.qk = -1;
            } else {
                r.qk = src_rob;
            }
        } else {
            r.vk = it->instr.ctrl.alu_src ? it->instr.imm : regs_.read(it->instr.rs2);
            r.qk = -1;
        }

        // Update rename table
        if (re.rd != 0 && it->instr.ctrl.reg_write) {
            reg_rename_[re.rd] = rob_id;
        }

        if (issued_this_cycle < 2) cycle_viz_.issue_asm[issued_this_cycle] = it->asm_str;
        issued_this_cycle++;
        it = instr_queue_.erase(it);
    }
    cycle_viz_.issued = issued_this_cycle;
    if (issued_this_cycle == 2) stats_.dual_issue_cycles++;
}

// ---------------------------------------------------------------------------
// PHASE: Execute (OoO modes — execute ready RS entries)
// ---------------------------------------------------------------------------
void SuperscalarPipeline::phase_execute() {
    for (int i = 0; i < 4; ++i) fu_results_[i] = {};
    for (int i = 0; i < 4; ++i) if (fu_busy_[i] > 0) fu_busy_[i]--;

    if (policy_ == SuperscalarPolicy::IO_IO) return; // IO/IO executes at issue time

    for (int i = 0; i < RS_COUNT; ++i) {
        if (!rs_[i].busy) continue;
        if (rs_[i].qj >= 0 || rs_[i].qk >= 0) continue; // operands not ready

        FUType fu = rs_[i].fu_type;
        int fu_idx = (int)fu;
        if (fu == FUType::ALU0 && fu_busy_[0] > 0) {
            fu = FUType::ALU1; fu_idx = 1;
        }
        if (fu_busy_[fu_idx] > 0) continue;

        int rob_id = rs_[i].rob_id;
        int32_t result = 0;

        if (fu == FUType::ALU0 || fu == FUType::ALU1) {
            result = execute_alu(rs_[i].op, rs_[i].vj, rs_[i].vk);
            if (rs_[i].ctrl.lui) result = rs_[i].imm;
            if (rs_[i].ctrl.auipc) result = (int32_t)(rs_[i].pc + rs_[i].imm);
            if (rs_[i].ctrl.jump || rs_[i].ctrl.jalr) result = (int32_t)(rs_[i].pc + 4);
        } else if (fu == FUType::MEM) {
            int32_t addr = rs_[i].vj + rs_[i].imm;
            if (rs_[i].ctrl.mem_read) {
                result = mem_.load((uint32_t)addr, MemSize::WORD, true);
                dcache_->access((uint32_t)addr, false);
            } else if (rs_[i].ctrl.mem_write) {
                mem_.store((uint32_t)addr, rs_[i].vk, MemSize::WORD);
                dcache_->access((uint32_t)addr, true);
            }
        } else if (fu == FUType::BRANCH) {
            if (rs_[i].ctrl.is_ecall) {
                // halt
            } else {
                bool taken = false; uint32_t target = 0;
                if (rs_[i].ctrl.branch) {
                    taken = eval_branch(rs_[i].ctrl.alu_op == ALUOp::SUB ? 0 : 1, rs_[i].vj, rs_[i].vk);
                    target = rs_[i].pc + rs_[i].imm;
                } else if (rs_[i].ctrl.jump) {
                    taken = true; target = rs_[i].pc + rs_[i].imm;
                } else if (rs_[i].ctrl.jalr) {
                    taken = true; target = (uint32_t)(rs_[i].vj + rs_[i].imm) & ~1u;
                }
                rob_[rob_id].branch_taken = taken;
                rob_[rob_id].branch_target = target;
                result = (int32_t)(rs_[i].pc + 4);
            }
        }

        rob_[rob_id].result = result;
        rob_[rob_id].ready = true;
        fu_busy_[fu_idx] = 1;
        fu_results_[fu_idx] = { rob_id, result, fu, true };

        // Free RS
        rs_[i].busy = false;
    }
}

// ---------------------------------------------------------------------------
// PHASE: Writeback — broadcast results on CDB (wakeup waiting RS entries)
// ---------------------------------------------------------------------------
void SuperscalarPipeline::phase_writeback() {
    cycle_viz_.completed = 0;
    for (int f = 0; f < 4; ++f) {
        if (!fu_results_[f].valid) continue;
        int rid = fu_results_[f].rob_id;
        int32_t val = fu_results_[f].value;
        cycle_viz_.completed++;

        // Wakeup all RS entries waiting on this ROB tag
        for (int i = 0; i < RS_COUNT; ++i) {
            if (!rs_[i].busy) continue;
            if (rs_[i].qj == rid) { rs_[i].vj = val; rs_[i].qj = -1; }
            if (rs_[i].qk == rid) { rs_[i].vk = val; rs_[i].qk = -1; }
        }
    }
}

// ---------------------------------------------------------------------------
// PHASE: Commit — retire instructions from ROB head (in program order)
// ---------------------------------------------------------------------------
void SuperscalarPipeline::phase_commit() {
    cycle_viz_.committed = 0;
    int committed_this = 0;
    int max_commit = (policy_ == SuperscalarPolicy::OOO_OOO) ? 2 : 2;

    while (committed_this < max_commit && rob_count_ > 0) {
        ROBEntry& head = rob_[rob_head_];
        if (!head.valid || !head.ready) break;

        // Write result to architectural register file
        if (head.rd != 0 && !head.is_store) {
            regs_.write(head.rd, head.result);
            // Clear rename if it still points to this ROB entry
            if (reg_rename_[head.rd] == rob_head_) {
                reg_rename_[head.rd] = -1;
            }
        }

        // Handle branch: redirect PC if taken
        if (head.is_branch && head.branch_taken) {
            pc_ = head.branch_target;
            // Flush instruction queue (speculative fetches)
            instr_queue_.clear();
            // Flush all RS entries and uncommitted ROB entries
            for (int i = 0; i < RS_COUNT; ++i) rs_[i].busy = false;
            // Mark remaining ROB entries as invalid
            int idx = (rob_head_ + 1) % ROB_SIZE;
            while (idx != rob_tail_) {
                if (rob_[idx].valid && rob_[idx].rd != 0) {
                    if (reg_rename_[rob_[idx].rd] == idx) reg_rename_[rob_[idx].rd] = -1;
                }
                rob_[idx] = {};
                idx = (idx + 1) % ROB_SIZE;
            }
            rob_tail_ = (rob_head_ + 1) % ROB_SIZE;
            rob_count_ = 1; // will be freed below

            cycle_viz_.flush = true;
            stats_.flush_cycles += 2;
            ++stats_.branch_count;
            ++stats_.mispredictions;
        }

        if (head.is_ecall) {
            halted_ = true;
        }

        // Track stats
        if (!head.is_store || head.is_load) {
            ++stats_.instructions;
        }

        rob_free_head();
        committed_this++;
    }
    cycle_viz_.committed = committed_this;
}

// ---------------------------------------------------------------------------
// step() — one cycle of the superscalar pipeline
// ---------------------------------------------------------------------------
nlohmann::json SuperscalarPipeline::step() {
    if (halted_) return build_state_json();

    ++cycle_;
    ++stats_.cycles;
    cycle_viz_ = {};

    // Execute phases in reverse pipeline order
    phase_commit();
    phase_writeback();
    phase_execute();
    phase_issue();
    phase_fetch_decode();

    return build_state_json();
}

nlohmann::json SuperscalarPipeline::run(int max_cycles) {
    nlohmann::json s;
    for (int i = 0; i < max_cycles && !halted_; ++i) {
        s = step();
    }
    return s;
}

// ---------------------------------------------------------------------------
// State serialization
// ---------------------------------------------------------------------------
nlohmann::json SuperscalarPipeline::build_state_json() const {
    // ROB entries
    nlohmann::json rob_json = nlohmann::json::array();
    for (int i = 0; i < ROB_SIZE; ++i) {
        int idx = (rob_head_ + i) % ROB_SIZE;
        if (i >= rob_count_) break;
        const auto& e = rob_[idx];
        if (!e.valid) continue;
        rob_json.push_back({
            {"id",        idx},
            {"pc",        hex32(e.pc)},
            {"asm",       e.asm_str},
            {"rd",        e.rd},
            {"result",    e.result},
            {"ready",     e.ready},
            {"committed", e.committed},
            {"is_branch", e.is_branch},
            {"fu",        (int)e.fu_used},
            {"is_head",   idx == rob_head_}
        });
    }

    // RS entries
    nlohmann::json rs_json = nlohmann::json::array();
    for (int i = 0; i < RS_COUNT; ++i) {
        if (!rs_[i].busy) continue;
        static const char* fu_names[] = {"ALU0","ALU1","MEM","BRU","?"};
        rs_json.push_back({
            {"id",      i},
            {"rob_id",  rs_[i].rob_id},
            {"asm",     rs_[i].asm_str},
            {"fu",      fu_names[(int)rs_[i].fu_type]},
            {"vj",      rs_[i].vj},
            {"vk",      rs_[i].vk},
            {"qj",      rs_[i].qj},
            {"qk",      rs_[i].qk},
            {"waiting",  rs_[i].qj >= 0 || rs_[i].qk >= 0}
        });
    }

    // FU status
    static const char* fu_names[] = {"ALU0","ALU1","MEM","BRU"};
    nlohmann::json fus = nlohmann::json::array();
    for (int i = 0; i < 4; ++i) {
        fus.push_back({
            {"name",  fu_names[i]},
            {"busy",  fu_busy_[i] > 0}
        });
    }

    // Instruction queue
    nlohmann::json iq_json = nlohmann::json::array();
    for (const auto& e : instr_queue_) {
        if (!e.valid) continue;
        iq_json.push_back({
            {"asm", e.asm_str},
            {"pc",  hex32(e.pc)}
        });
    }

    // Policy name
    std::string policy_str;
    switch (policy_) {
        case SuperscalarPolicy::IO_IO:   policy_str = "io_io"; break;
        case SuperscalarPolicy::OOO_IO:  policy_str = "ooo_io"; break;
        case SuperscalarPolicy::OOO_OOO: policy_str = "ooo_ooo"; break;
    }

    // Stats
    nlohmann::json st = stats_.to_json();
    st["dual_issue_cycles"]  = stats_.dual_issue_cycles;
    st["ipc_max"]            = 2.0;

    return {
        {"engine",            "superscalar"},
        {"superscalar_policy", policy_str},
        {"cycle",             cycle_},
        {"halted",            halted_},
        {"stall",             cycle_viz_.stall},
        {"flush",             cycle_viz_.flush},
        {"fetch_slots",       {{cycle_viz_.fetch_asm[0]}, {cycle_viz_.fetch_asm[1]}}},
        {"issue_slots",       {{cycle_viz_.issue_asm[0]}, {cycle_viz_.issue_asm[1]}}},
        {"fetched",           cycle_viz_.fetched},
        {"issued",            cycle_viz_.issued},
        {"completed",         cycle_viz_.completed},
        {"committed",         cycle_viz_.committed},
        {"rob",               rob_json},
        {"rob_size",          ROB_SIZE},
        {"rob_count",         rob_count_},
        {"reservation_stations", rs_json},
        {"functional_units",  fus},
        {"instr_queue",       iq_json},
        {"stats",             st},
        {"registers",         regs_.to_json()},
        {"cache",             {{"icache", icache_->get_state()}, {"dcache", dcache_->get_state()}}},
        {"predictor",         predictor_->get_state()}
    };
}

nlohmann::json SuperscalarPipeline::get_state_json() const {
    return build_state_json();
}

void SuperscalarPipeline::configure(const nlohmann::json& cfg) {
    if (cfg.contains("predictor")) {
        init_predictor(cfg["predictor"].get<std::string>());
    }
    if (cfg.contains("superscalar_policy")) {
        std::string p = cfg["superscalar_policy"].get<std::string>();
        if (p == "io_io")         policy_ = SuperscalarPolicy::IO_IO;
        else if (p == "ooo_io")   policy_ = SuperscalarPolicy::OOO_IO;
        else if (p == "ooo_ooo")  policy_ = SuperscalarPolicy::OOO_OOO;
    }
}

} // namespace rv32i
