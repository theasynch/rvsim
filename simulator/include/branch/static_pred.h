// static_pred.h + static_pred.cpp — Always-Taken and Always-Not-Taken predictors
#pragma once
#include "branch/predictor.h"

namespace rv32i {

// Always predicts NOT taken (simplest possible predictor)
// Accuracy: good for straight-line code, poor for loops (loop-back always mispredicted)
class StaticNotTakenPredictor : public Predictor {
public:
    bool predict(uint32_t) override { ++predictions; return false; }
    void update(uint32_t, bool taken) override { if (taken) ++mispredictions; }
    std::string name() const override { return "Static Not-Taken"; }
    void reset() override { predictions = 0; mispredictions = 0; }
    nlohmann::json get_state() const override {
        return {{"type","static_not_taken"},{"prediction","never taken"}};
    }
};

// Always predicts TAKEN (good for loop back-edges, bad for forward branches)
class StaticTakenPredictor : public Predictor {
public:
    bool predict(uint32_t) override { ++predictions; return true; }
    void update(uint32_t, bool taken) override { if (!taken) ++mispredictions; }
    std::string name() const override { return "Static Taken"; }
    void reset() override { predictions = 0; mispredictions = 0; }
    nlohmann::json get_state() const override {
        return {{"type","static_taken"},{"prediction","always taken"}};
    }
};

} // namespace rv32i
