// tournament_pred.h — Tournament (Hybrid) Branch Predictor
#pragma once
#include "branch/predictor.h"
#include "branch/two_bit_pred.h"
#include "branch/gshare_pred.h"
#include <vector>

namespace rv32i {
// =============================================================================
// Tournament predictor: combines a LOCAL predictor and a GLOBAL predictor,
// with a CHOOSER that learns which one is more accurate for each branch PC.
//
// Used in: Alpha 21264, AMD processors.
//
//   local_pred  → [  CHOOSER  ] → final prediction
//   global_pred →  (2-bit per PC)
//
// Chooser update rule:
//   If local correct, global wrong  → increment chooser (prefer local)
//   If global correct, local wrong  → decrement chooser (prefer global)
//   If both correct or both wrong   → no change
// =============================================================================
class TournamentPredictor : public Predictor {
public:
    TournamentPredictor(int local_size = 256, int global_history = 8);

    bool predict(uint32_t pc) override;
    void update(uint32_t pc, bool taken) override;
    std::string name() const override { return "Tournament"; }
    void reset() override;
    nlohmann::json get_state() const override;

private:
    TwoBitPredictor local_;
    GSharePredictor global_;
    std::vector<uint8_t> chooser_;  // 2-bit saturating counters
    int chooser_size_;
    int chooser_mask_;

    // Store last predictions for update logic
    bool last_local_pred_  = false;
    bool last_global_pred_ = false;
    uint32_t last_pc_      = 0;

    int chooser_index(uint32_t pc) const {
        return (pc >> 2) & chooser_mask_;
    }
};

} // namespace rv32i
