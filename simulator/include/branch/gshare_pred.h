// gshare_pred.h — GShare Branch Predictor
#pragma once
#include "branch/predictor.h"
#include <vector>

namespace rv32i {
// =============================================================================
// GShare: XOR the PC with the Global History Register (GHR) to index the PHT.
//
// Global History Register: tracks the outcomes of the last N branches.
//   GHR = {branch_N-1, branch_N-2, ..., branch_N-k}  (1=taken, 0=not-taken)
//
// Index = PC[k+1:2] XOR GHR
//
// Why XOR? It spreads entries across the PHT, reducing aliasing (two different
// branches mapping to the same PHT entry). GShare exploits correlation between
// the current branch and the recent history of branches.
// =============================================================================
class GSharePredictor : public Predictor {
public:
    explicit GSharePredictor(int history_bits = 8);  // 8-bit GHR → 256-entry PHT

    bool predict(uint32_t pc) override;
    void update(uint32_t pc, bool taken) override;
    std::string name() const override { return "GShare"; }
    void reset() override;
    nlohmann::json get_state() const override;

private:
    int history_bits_;
    uint32_t ghr_;      // Global History Register
    uint32_t ghr_mask_;
    std::vector<uint8_t> pht_;  // Pattern History Table (2-bit saturating counters)

    int index(uint32_t pc) const {
        return ((pc >> 2) ^ ghr_) & ghr_mask_;
    }
};

} // namespace rv32i
