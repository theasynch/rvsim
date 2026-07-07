// tournament_pred.cpp
#include "branch/tournament_pred.h"

namespace rv32i {

TournamentPredictor::TournamentPredictor(int local_size, int global_history)
    : local_(local_size), global_(global_history),
      chooser_(local_size, 1), // Initialize to neutral (weakly prefer local)
      chooser_size_(local_size), chooser_mask_(local_size - 1)
{}

bool TournamentPredictor::predict(uint32_t pc) {
    ++predictions;
    last_pc_         = pc;
    last_local_pred_ = local_.predict(pc);
    last_global_pred_= global_.predict(pc);

    // Chooser: 0,1 = prefer local; 2,3 = prefer global
    int idx = chooser_index(pc);
    if (chooser_[idx] >= 2)
        return last_global_pred_;
    else
        return last_local_pred_;
}

void TournamentPredictor::update(uint32_t pc, bool taken) {
    bool local_correct  = (last_local_pred_  == taken);
    bool global_correct = (last_global_pred_ == taken);

    int idx = chooser_index(pc);

    // Update chooser
    if (local_correct && !global_correct) {
        // Local was better → prefer local (decrement toward 0)
        if (chooser_[idx] > 0) --chooser_[idx];
    } else if (!local_correct && global_correct) {
        // Global was better → prefer global (increment toward 3)
        if (chooser_[idx] < 3) ++chooser_[idx];
    }

    // Was the final prediction correct?
    bool final_pred = (chooser_[idx] >= 2) ? last_global_pred_ : last_local_pred_;
    if (final_pred != taken) ++mispredictions;

    local_.update(pc, taken);
    global_.update(pc, taken);
}

void TournamentPredictor::reset() {
    local_.reset(); global_.reset();
    std::fill(chooser_.begin(), chooser_.end(), 1);
    predictions = 0; mispredictions = 0;
}

nlohmann::json TournamentPredictor::get_state() const {
    nlohmann::json j;
    j["type"]         = "tournament";
    j["local"]        = local_.get_state();
    j["global"]       = global_.get_state();
    j["mispred_rate"] = misprediction_rate();
    nlohmann::json ch = nlohmann::json::array();
    int show = std::min(chooser_size_, 32);
    for (int i = 0; i < show; ++i)
        ch.push_back({{"index",i},{"value",chooser_[i]},
                      {"prefers", chooser_[i]>=2?"global":"local"}});
    j["chooser"] = ch;
    return j;
}

} // namespace rv32i
