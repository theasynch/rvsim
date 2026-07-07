// two_bit_pred.cpp
#include "branch/two_bit_pred.h"

namespace rv32i {

TwoBitPredictor::TwoBitPredictor(int pht_size)
    : pht_size_(pht_size), index_mask_(pht_size - 1),
      pht_(pht_size, 0b01) // Initialize all to Weakly Not Taken
{}

bool TwoBitPredictor::predict(uint32_t pc) {
    ++predictions;
    int idx = index(pc);
    return pht_[idx] >= 2; // Predict TAKEN if counter >= 2 (WT or ST)
}

void TwoBitPredictor::update(uint32_t pc, bool taken) {
    int idx = index(pc);
    bool predicted = (pht_[idx] >= 2);
    if (predicted != taken) ++mispredictions;

    if (taken && pht_[idx] < 3) ++pht_[idx];  // Saturate at 3 (ST)
    if (!taken && pht_[idx] > 0) --pht_[idx]; // Saturate at 0 (SNT)
}

void TwoBitPredictor::reset() {
    std::fill(pht_.begin(), pht_.end(), 0b01);
    predictions = 0; mispredictions = 0;
}

nlohmann::json TwoBitPredictor::get_state() const {
    static const char* state_names[] = {"SNT","WNT","WT","ST"};
    nlohmann::json j;
    j["type"] = "two_bit";
    j["pht_size"] = pht_size_;
    // Return first 32 entries for visualization
    nlohmann::json entries = nlohmann::json::array();
    int show = std::min(pht_size_, 32);
    for (int i = 0; i < show; ++i) {
        entries.push_back({{"index",i},{"value",pht_[i]},{"state",state_names[pht_[i]]}});
    }
    j["pht_entries"] = entries;
    j["mispred_rate"] = misprediction_rate();
    return j;
}

} // namespace rv32i
