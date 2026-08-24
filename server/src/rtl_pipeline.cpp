#include "rtl_pipeline.h"
#include "Vrv32i_top.h"
#include "Vrv32i_top___024root.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "isa/rv32i_types.h"
#include <sstream>
#include <iomanip>
#include <iostream>

// Required by Verilator
double sc_time_stamp() { return 0; }

namespace rv32i {

RtlPipeline::RtlPipeline(PipelineConfig cfg) : config_(cfg), cycle_(0), halted_(false) {
    top_ = std::make_unique<Vrv32i_top>();
    // We disable tracing by default for performance, but it can be enabled if needed
    // Verilated::traceEverOn(true);
    // tfp_ = std::make_unique<VerilatedVcdC>();
    // top_->trace(tfp_.get(), 99);
    // tfp_->open("waves.vcd");
    
    reset();
}

RtlPipeline::~RtlPipeline() {
    top_->final();
    if (tfp_) {
        tfp_->close();
    }
}

void RtlPipeline::tick() {
    top_->clk = 1;
    top_->eval();
    if (tfp_) tfp_->dump(cycle_ * 10 + 5);

    top_->clk = 0;
    top_->eval();
    if (tfp_) tfp_->dump(cycle_ * 10 + 10);
    
    cycle_++;
}

void RtlPipeline::reset() {
    top_->rst_n = 0;
    top_->clk = 0;
    top_->eval();
    tick(); // clock reset
    top_->rst_n = 1;
    top_->eval();
    cycle_ = 0;
    halted_ = false;
}

bool RtlPipeline::load_program(const std::string& hex_str) {
    reset();
    std::istringstream iss(hex_str);
    std::string line;
    uint32_t addr = 0;
    
    // Clear imem to 0 first
    for(int i=0; i<1024; i++) {
        top_->rootp->rv32i_top__DOT__u_imem__DOT__mem[i] = 0;
    }

    while (std::getline(iss, line)) {
        // Strip carriage returns/spaces
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        if (line.empty()) continue;

        try {
            uint32_t inst = std::stoul(line, nullptr, 16);
            if (addr < 1024) {
                top_->rootp->rv32i_top__DOT__u_imem__DOT__mem[addr++] = inst;
            }
        } catch (...) {
            return false;
        }
    }
    
    return true;
}

PipelineState RtlPipeline::get_state() const {
    return build_state();
}

PipelineState RtlPipeline::step() {
    if (!halted_) {
        tick();
        if (top_->halt) {
            halted_ = true;
        }
    }
    return build_state();
}

PipelineState RtlPipeline::run(int max_cycles) {
    int count = 0;
    while (!halted_ && count < max_cycles) {
        step();
        count++;
    }
    return build_state();
}

void RtlPipeline::configure(const PipelineConfig& cfg) {
    config_ = cfg;
}

PipelineState RtlPipeline::build_state() const {
    PipelineState s;
    s.cycle = cycle_;
    s.halted = halted_ || top_->halt;
    
    // We map Verilog debug signals to the pipeline state
    s.stall_this_cycle = top_->stall_if;
    s.flush_this_cycle = top_->flush_id;

    // IF Stage
    s.stages[0].stage = "IF";
    s.stages[0].pc = top_->debug_pc_if;
    
    // ID Stage
    s.stages[1].stage = "ID";
    s.stages[1].pc = top_->debug_pc_id;
    s.stages[1].bubble = top_->flush_id;
    
    // EX Stage
    s.stages[2].stage = "EX";
    s.stages[2].pc = top_->debug_pc_ex;
    
    // MEM Stage
    s.stages[3].stage = "MEM";
    s.stages[3].pc = top_->debug_pc_mem;
    
    // WB Stage
    s.stages[4].stage = "WB";
    s.stages[4].pc = top_->debug_pc_wb;
    s.stages[4].wb_rd = top_->debug_reg_addr;
    s.stages[4].wb_value = top_->debug_reg_data;
    s.stages[4].wb_active = top_->debug_reg_write;

    // Format hex strings
    for (int i=0; i<5; i++) {
        std::stringstream ss;
        ss << "0x" << std::setfill('0') << std::setw(8) << std::hex << s.stages[i].pc;
        s.stages[i].pc_hex = ss.str();
    }
    
    // Registers mapping
    s.registers = nlohmann::json::array();
    for (int i=0; i<32; i++) {
        nlohmann::json reg_obj;
        reg_obj["name"] = "x" + std::to_string(i);
        reg_obj["value"] = top_->rootp->rv32i_top__DOT__u_regfile__DOT__regs[i];
        s.registers.push_back(reg_obj);
    }
    
    return s;
}

} // namespace rv32i
