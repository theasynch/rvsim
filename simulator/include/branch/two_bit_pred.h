// =============================================================================
// two_bit_pred.h — 2-Bit Saturating Counter Branch Predictor
// =============================================================================
//
// The 2-bit predictor uses a Pattern History Table (PHT): an array of 2-bit
// saturating counters indexed by PC bits. Each counter has 4 states:
//
//   00 = Strongly Not Taken (SNT)  → predict NOT taken
//   01 = Weakly Not Taken (WNT)    → predict NOT taken
//   10 = Weakly Taken (WT)         → predict TAKEN
//   11 = Strongly Taken (ST)       → predict TAKEN
//
// State transitions:
//   On TAKEN:     00→01, 01→10, 10→11, 11→11 (saturate at 11)
//   On NOT TAKEN: 11→10, 10→01, 01→00, 00→00 (saturate at 00)
//
// The 2-bit design improves over 1-bit by handling the "loop exit" case:
// a loop that runs 100 times only mispredicts TWICE (first iteration and exit)
// vs TWICE per iteration with a 1-bit predictor.
// =============================================================================
#pragma once
#include "branch/predictor.h"
#include <vector>

namespace rv32i {

class TwoBitPredictor : public Predictor {
public:
    // pht_size: number of entries in the PHT (must be power of 2)
    // PC is indexed by the low log2(pht_size) bits
    explicit TwoBitPredictor(int pht_size = 256);

    bool predict(uint32_t pc) override;
    void update(uint32_t pc, bool taken) override;
    std::string name() const override { return "2-Bit Saturating Counter"; }
    void reset() override;
    nlohmann::json get_state() const override;

private:
    int pht_size_;
    int index_mask_;
    std::vector<uint8_t> pht_; // 0..3 (2-bit saturating counter)

    int index(uint32_t pc) const { return (pc >> 2) & index_mask_; }
};

} // namespace rv32i
