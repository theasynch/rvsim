// =============================================================================
// predictor.h — Abstract Branch Predictor Interface
// =============================================================================
//
// In hardware, branch prediction lives in the IF stage. When the PC is
// presented to the instruction cache, the branch predictor simultaneously
// looks up whether this PC is likely a branch, and if so, which direction.
//
// In our pipeline, we call predict() in IF and update() in EX (when the
// branch outcome is finally known).
//
// Misprediction penalty: 2 cycles (flush IF + ID stages)
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

namespace rv32i {

class Predictor {
public:
    virtual ~Predictor() = default;

    // predict() — called in IF stage.
    // Returns true if we predict the branch WILL be taken.
    virtual bool predict(uint32_t pc) = 0;

    // update() — called in EX stage once actual outcome is known.
    // pc: address of the branch instruction
    // taken: actual outcome
    virtual void update(uint32_t pc, bool taken) = 0;

    // Name of this predictor (for display)
    virtual std::string name() const = 0;

    // State for visualization (PHT entries, history register, etc.)
    virtual nlohmann::json get_state() const = 0;

    // Reset predictor state
    virtual void reset() = 0;

    // Stats
    uint64_t predictions   = 0;
    uint64_t mispredictions = 0;

    double misprediction_rate() const {
        return predictions > 0 ? (double)mispredictions / predictions : 0.0;
    }
};

} // namespace rv32i
