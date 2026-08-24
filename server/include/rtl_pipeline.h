#pragma once
#include "core/pipeline.h"
#include <memory>
#include <string>

class Vrv32i_top;
class VerilatedVcdC;

namespace rv32i {

class RtlPipeline {
public:
    explicit RtlPipeline(PipelineConfig cfg = {});
    ~RtlPipeline();

    PipelineState step();
    PipelineState run(int max_cycles = 100000);
    bool load_program(const std::string& hex_str);
    void reset();
    PipelineState get_state() const;
    void configure(const PipelineConfig& cfg);

    bool is_halted() const { return halted_; }
    uint64_t cycle_count() const { return cycle_; }

private:
    std::unique_ptr<Vrv32i_top> top_;
    std::unique_ptr<VerilatedVcdC> tfp_;
    uint64_t cycle_;
    bool halted_;
    PipelineConfig config_;

    void tick();
    PipelineState build_state() const;
};

} // namespace rv32i
