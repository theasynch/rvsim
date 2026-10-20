// =============================================================================
// vliw_pipeline.cpp — VLIW RISC-V Pipeline Implementation
// =============================================================================
#include "core/vliw_pipeline.h"
#include "branch/static_pred.h"
#include "branch/two_bit_pred.h"
#include "branch/gshare_pred.h"
#include "branch/tournament_pred.h"
#include <sstream>

namespace rv32i {

static std::string hex32(uint32_t v) {
    char buf[12];
    snprintf(buf, sizeof(buf), "0x%08X", v);
    return buf;
}

std::string VLIWBundle::to_string() const {
    if (!valid) return "{ NOP | NOP | NOP }";
    std::string s = "{ ";
    s += slots[0].is_nop ? "NOP" : slots[0].asm_str;
    s += " | ";
    s += slots[1].is_nop ? "NOP" : slots[1].asm_str;
    s += " | ";
    s += slots[2].is_nop ? "NOP" : slots[2].asm_str;
    s += " }";
    return s;
}

VLIWPipeline::VLIWPipeline() {
    init_predictor("static_not_taken");
    init_caches();
    reset();
}

void VLIWPipeline::init_predictor(const std::string& type) {
    if (type == "static_taken")       predictor_ = std::make_unique<StaticTakenPredictor>();
    else if (type == "two_bit")       predictor_ = std::make_unique<TwoBitPredictor>(256);
    else if (type == "gshare")        predictor_ = std::make_unique<GSharePredictor>(8);
    else if (type == "tournament")    predictor_ = std::make_unique<TournamentPredictor>(256, 8);
    else                              predictor_ = std::make_unique<StaticNotTakenPredictor>();
}

void VLIWPipeline::init_caches() {
    CacheConfig cfg;
    cfg.size_bytes = 8*1024; cfg.block_size = 64;
    cfg.associativity = 4; cfg.policy = ReplacementPolicy::LRU;
    
    cfg.name = "I-Cache";
    icache_ = std::make_unique<Cache>(cfg);
    
    cfg.name = "D-Cache";
    dcache_ = std::make_unique<Cache>(cfg);
}

void VLIWPipeline::reset() {
    pc_ = 0;
    bundle_idx_ = 0;
    cycle_ = 0;
    halted_ = false;
    total_slots_ = 0;
    used_slots_ = 0;
    regs_.reset();
    regs_.write(2, (int32_t)0x7FFFFFF0);
    stats_.reset();
    icache_->reset();
    dcache_->reset();
    predictor_->reset();
    for (int i = 0; i < 5; ++i) pipe_[i] = {};
    cycle_viz_ = {};
}

// ---------------------------------------------------------------------------
// VLIW Bundler
// ---------------------------------------------------------------------------
int VLIWPipeline::classify_slot(const DecodedInstr& instr) const {
    if (instr.ctrl.branch || instr.ctrl.jump || instr.ctrl.jalr) return 2; // BRU
    if (instr.ctrl.mem_read || instr.ctrl.mem_write) return 1;             // MEM
    return 0; // ALU
}

bool VLIWPipeline::has_dependency(const DecodedInstr& cur, const DecodedInstr& prev) const {
    // RAW, WAW, WAR hazard checks within a bundle
    if (prev.ctrl.reg_write && prev.rd != 0) {
        if (cur.rs1 == prev.rd) return true;
        if (cur.rs2 == prev.rd && !cur.ctrl.alu_src) return true;
        if (cur.ctrl.reg_write && cur.rd == prev.rd) return true; // WAW
    }
    if (cur.ctrl.reg_write && cur.rd != 0) {
        if (prev.rs1 == cur.rd) return true; // WAR
        if (prev.rs2 == cur.rd && !prev.ctrl.alu_src) return true; // WAR
    }
    // Memory dependencies (store followed by load/store to same/unknown address)
    // For static bundling, we assume any two memory ops might alias, 
    // but we only have 1 MEM slot anyway, so we don't need to check.
    return false;
}

bool VLIWPipeline::load_program(const std::string& hex_str) {
    reset();
    bool ok = mem_.load_hex(hex_str);
    if (!ok) return false;
    
    bundle_program();
    return true;
}

void VLIWPipeline::bundle_program() {
    program_bundles_.clear();
    
    // Parse all instructions first
    std::vector<DecodedInstr> instrs;
    std::vector<uint32_t> pcs;
    
    uint32_t curr_pc = 0;
    while (curr_pc < MEM_SIZE) {
        uint32_t word = mem_.load(curr_pc, MemSize::WORD, false);
        if (word == 0 && curr_pc > 0) break; // End of program (heuristically)
        
        DecodedInstr di = decoder_.decode(word, curr_pc);
        if (di.valid) {
            instrs.push_back(di);
            pcs.push_back(curr_pc);
        }
        curr_pc += 4;
    }
    
    // Greedy bundling
    VLIWBundle curr_bundle;
    curr_bundle.bundle_pc = instrs.empty() ? 0 : pcs[0];
    
    for (size_t i = 0; i < instrs.size(); ++i) {
        const auto& instr = instrs[i];
        int slot = classify_slot(instr);
        
        bool can_pack = true;
        
        // Slot taken?
        if (curr_bundle.slots[slot].valid) {
            can_pack = false;
        } else {
            // Check dependencies against instructions already in bundle
            for (int s = 0; s < 3; ++s) {
                if (curr_bundle.slots[s].valid && !curr_bundle.slots[s].is_nop) {
                    if (has_dependency(instr, curr_bundle.slots[s].instr)) {
                        can_pack = false;
                        break;
                    }
                }
            }
        }
        
        if (!can_pack) {
            curr_bundle.valid = true;
            program_bundles_.push_back(curr_bundle);
            curr_bundle = VLIWBundle(); // Reset
            curr_bundle.bundle_pc = pcs[i];
        }
        
        curr_bundle.slots[slot].instr = instr;
        curr_bundle.slots[slot].pc = pcs[i];
        curr_bundle.slots[slot].asm_str = instr.mnemonic;
        curr_bundle.slots[slot].valid = true;
        curr_bundle.slots[slot].is_nop = false;
        
        // A branch ends a bundle (simplifies control flow)
        if (instr.ctrl.branch || instr.ctrl.jump || instr.ctrl.jalr) {
            curr_bundle.valid = true;
            program_bundles_.push_back(curr_bundle);
            if (i + 1 < instrs.size()) {
                curr_bundle = VLIWBundle();
                curr_bundle.bundle_pc = pcs[i+1];
            } else {
                curr_bundle = VLIWBundle(); // empty trailing
            }
        }
    }
    
    if (curr_bundle.slots[0].valid || curr_bundle.slots[1].valid || curr_bundle.slots[2].valid) {
        curr_bundle.valid = true;
        program_bundles_.push_back(curr_bundle);
    }
}

// ---------------------------------------------------------------------------
// Pipeline Stages
// ---------------------------------------------------------------------------
int32_t VLIWPipeline::execute_alu(ALUOp op, int32_t a, int32_t b) const {
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

bool VLIWPipeline::eval_branch(uint8_t funct3, int32_t rs1, int32_t rs2) const {
    uint32_t u1 = (uint32_t)rs1, u2 = (uint32_t)rs2;
    switch (funct3) {
        case 0: return rs1 == rs2;
        case 1: return rs1 != rs2;
        case 4: return rs1 < rs2;
        case 5: return rs1 >= rs2;
        case 6: return u1 < u2;
        case 7: return u1 >= u2;
        default: return false;
    }
}

void VLIWPipeline::stage_wb() {
    auto& pr = pipe_[4]; // MEM/WB
    if (!pr.valid || pr.bubble) return;
    
    // ALU slot WB
    if (!pr.bundle.slots[0].is_nop) {
        auto& instr = pr.bundle.slots[0].instr;
        if (instr.ctrl.reg_write && instr.rd != 0) {
            regs_.write(instr.rd, (int32_t)pr.bundle.slots[0].pc); // We hijacked PC for result carrying
            // Note: in a real implementation we'd expand VLIWSlot to hold results, 
            // but for brevity we'll just execute and commit directly in EX/MEM stages 
            // since this is a pure behavioral simulator.
        }
    }
    
    // MEM slot WB
    if (!pr.bundle.slots[1].is_nop) {
         auto& instr = pr.bundle.slots[1].instr;
         if (instr.ctrl.mem_to_reg && instr.ctrl.reg_write && instr.rd != 0) {
              regs_.write(instr.rd, (int32_t)pr.bundle.slots[1].pc);
         }
    }
}

void VLIWPipeline::stage_mem() {
    auto& pr = pipe_[3]; // EX/MEM
    auto& next_pr = pipe_[4];
    next_pr = pr;
    
    if (!pr.valid || pr.bubble) return;
    
    // MEM slot
    if (!pr.bundle.slots[1].is_nop) {
        auto& instr = pr.bundle.slots[1].instr;
        int32_t rs2_val = regs_.read(instr.rs2); // Naive: no forwarding in this basic model
        int32_t addr = regs_.read(instr.rs1) + instr.imm;
        
        if (instr.ctrl.mem_read) {
            int32_t val = mem_.load((uint32_t)addr, MemSize::WORD, true);
            dcache_->access((uint32_t)addr, false);
            next_pr.bundle.slots[1].pc = val; // Hijack pc to pass result to WB
        } else if (instr.ctrl.mem_write) {
            mem_.store((uint32_t)addr, rs2_val, MemSize::WORD);
            dcache_->access((uint32_t)addr, true);
        }
    }
}

void VLIWPipeline::stage_ex() {
    auto& pr = pipe_[2]; // ID/EX
    auto& next_pr = pipe_[3];
    next_pr = pr;
    
    if (!pr.valid || pr.bubble) return;
    
    // ALU slot
    if (!pr.bundle.slots[0].is_nop) {
        auto& instr = pr.bundle.slots[0].instr;
        int32_t a = regs_.read(instr.rs1);
        int32_t b = instr.ctrl.alu_src ? instr.imm : regs_.read(instr.rs2);
        int32_t res = execute_alu(instr.ctrl.alu_op, a, b);
        if (instr.ctrl.lui) res = instr.imm;
        if (instr.ctrl.auipc) res = (int32_t)(pr.bundle.slots[0].pc + instr.imm);
        next_pr.bundle.slots[0].pc = res; // Hijack to pass result
        
        if (instr.ctrl.is_ecall) halted_ = true;
    }
    
    // BRU slot
    if (!pr.bundle.slots[2].is_nop) {
        auto& instr = pr.bundle.slots[2].instr;
        int32_t a = regs_.read(instr.rs1);
        int32_t b = regs_.read(instr.rs2);
        
        bool taken = false;
        uint32_t target = 0;
        
        if (instr.ctrl.branch) {
            taken = eval_branch(instr.funct3, a, b);
            target = pr.bundle.slots[2].pc + instr.imm;
        } else if (instr.ctrl.jump) {
            taken = true;
            target = pr.bundle.slots[2].pc + instr.imm;
        } else if (instr.ctrl.jalr) {
            taken = true;
            target = (uint32_t)(a + instr.imm) & ~1u;
        }
        
        if (taken) {
            // Flush IF and ID
            pipe_[0].bubble = true;
            pipe_[1].bubble = true;
            
            // Find bundle containing target
            bundle_idx_ = 0;
            for (size_t i = 0; i < program_bundles_.size(); ++i) {
                if (program_bundles_[i].bundle_pc >= target) {
                    bundle_idx_ = i;
                    break;
                }
            }
            
            cycle_viz_.flush = true;
            stats_.flush_cycles += 2;
        }
        
        if (instr.ctrl.jump || instr.ctrl.jalr) {
            // Write link address
             if (instr.ctrl.reg_write && instr.rd != 0) {
                 regs_.write(instr.rd, pr.bundle.slots[2].pc + 4);
             }
        }
    }
}

void VLIWPipeline::stage_id() {
    auto& pr = pipe_[1]; // IF/ID
    auto& next_pr = pipe_[2];
    
    // Decode is already done by the bundler for us, we just pass it along
    next_pr = pr;
    
    if (pr.valid && !pr.bubble) {
        // Track stats
        for (int i=0; i<3; ++i) {
            if (!pr.bundle.slots[i].is_nop) {
                stats_.instructions++;
            }
        }
    }
}

void VLIWPipeline::stage_if() {
    auto& pr = pipe_[0]; // Fetch
    
    if (bundle_idx_ < (int)program_bundles_.size()) {
        pr.bundle = program_bundles_[bundle_idx_];
        pr.valid = true;
        pr.bubble = false;
        bundle_idx_++;
        icache_->access(pr.bundle.bundle_pc, false);
    } else {
        pr.valid = false;
        pr.bubble = true;
    }
}

nlohmann::json VLIWPipeline::step() {
    if (halted_) return build_state_json();
    
    cycle_++;
    stats_.cycles++;
    cycle_viz_ = {};
    
    // Shift pipeline backwards
    stage_wb();
    stage_mem();
    stage_ex();
    stage_id();
    stage_if();
    
    // Stats update
    total_slots_ += 3;
    if (pipe_[0].valid && !pipe_[0].bubble) {
        for(int i=0; i<3; ++i) {
            if (!pipe_[0].bundle.slots[i].is_nop) used_slots_++;
        }
    }
    
    return build_state_json();
}

nlohmann::json VLIWPipeline::run(int max_cycles) {
    nlohmann::json s;
    for (int i = 0; i < max_cycles && !halted_; ++i) {
        s = step();
    }
    return s;
}

nlohmann::json VLIWPipeline::build_state_json() const {
    nlohmann::json stages = nlohmann::json::array();
    const char* snames[] = {"IF", "ID", "EX", "MEM", "WB"};
    
    for (int i=0; i<5; ++i) {
        nlohmann::json slots_json = nlohmann::json::array();
        for (int s=0; s<3; ++s) {
            slots_json.push_back({
                {"asm", pipe_[i].bubble || !pipe_[i].valid ? "---" : (pipe_[i].bundle.slots[s].is_nop ? "NOP" : pipe_[i].bundle.slots[s].asm_str)}
            });
        }
        
        stages.push_back({
            {"stage", snames[i]},
            {"bubble", pipe_[i].bubble},
            {"slots", slots_json}
        });
    }
    
    float util = total_slots_ > 0 ? (float)used_slots_ / total_slots_ * 100.0f : 0.0f;
    
    nlohmann::json st = stats_.to_json();
    st["vliw_utilization"] = util;
    st["ipc_max"] = 3.0;

    return {
        {"engine",            "vliw"},
        {"cycle",             cycle_},
        {"halted",            halted_},
        {"stall",             cycle_viz_.stall},
        {"flush",             cycle_viz_.flush},
        {"stages",            stages},
        {"stats",             st},
        {"registers",         regs_.to_json()},
        {"cache",             {{"icache", icache_->get_state()}, {"dcache", dcache_->get_state()}}},
        {"predictor",         predictor_->get_state()}
    };
}

nlohmann::json VLIWPipeline::get_state_json() const {
    return build_state_json();
}

void VLIWPipeline::configure(const nlohmann::json& cfg) {
    if (cfg.contains("predictor")) {
        init_predictor(cfg["predictor"].get<std::string>());
    }
}

} // namespace rv32i
