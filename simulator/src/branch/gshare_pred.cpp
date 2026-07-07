// gshare_pred.cpp
#include "branch/gshare_pred.h"

namespace rv32i {

GSharePredictor::GSharePredictor(int history_bits)
    : history_bits_(history_bits),
      ghr_(0),
      ghr_mask_((1u << history_bits) - 1),
      pht_(1u << history_bits, 0b01) // Weakly Not Taken initial state
{}

bool GSharePredictor::predict(uint32_t pc) {
    ++predictions;
    return pht_[index(pc)] >= 2;
}

void GSharePredictor::update(uint32_t pc, bool taken) {
    int idx = index(pc);
    bool predicted = (pht_[idx] >= 2);
    if (predicted != taken) ++mispredictions;

    // Update PHT
    if (taken  && pht_[idx] < 3) ++pht_[idx];
    if (!taken && pht_[idx] > 0) --pht_[idx];

    // Shift GHR left, insert new outcome at bit 0
    ghr_ = ((ghr_ << 1) | (taken ? 1 : 0)) & ghr_mask_;
}

void GSharePredictor::reset() {
    ghr_ = 0;
    std::fill(pht_.begin(), pht_.end(), 0b01);
    predictions = 0; mispredictions = 0;
}

nlohmann::json GSharePredictor::get_state() const {
    static const char* state_names[] = {"SNT","WNT","WT","ST"};
    nlohmann::json j;
    j["type"]         = "gshare";
    j["history_bits"] = history_bits_;
    j["ghr"]          = ghr_;
    j["ghr_binary"]   = [&]() {
        std::string s;
        for (int i = history_bits_ - 1; i >= 0; --i)
            s += ((ghr_ >> i) & 1) ? '1' : '0';
        return s;
    }();
    j["pht_size"] = (int)pht_.size();
    nlohmann::json entries = nlohmann::json::array();
    int show = std::min((int)pht_.size(), 32);
    for (int i = 0; i < show; ++i)
        entries.push_back({{"index",i},{"value",pht_[i]},{"state",state_names[pht_[i]]}});
    j["pht_entries"] = entries;
    j["mispred_rate"] = misprediction_rate();
    return j;
}

} // namespace rv32i
