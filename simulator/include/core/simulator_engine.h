// =============================================================================
// simulator_engine.h — Abstract interface for all processor simulation models
// =============================================================================
//
// Every simulator (scalar pipeline, superscalar, VLIW) implements this
// interface. The API server holds a unique_ptr<SimulatorEngine> and can
// hot-swap between engines at runtime.
//
// =============================================================================
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <cstdint>

namespace rv32i {

class SimulatorEngine {
public:
    virtual ~SimulatorEngine() = default;

    // -----------------------------------------------------------------------
    // step() — advance by exactly ONE clock cycle. Returns state as JSON.
    // -----------------------------------------------------------------------
    virtual nlohmann::json step() = 0;

    // -----------------------------------------------------------------------
    // run() — advance up to max_cycles or until halted. Returns final state.
    // -----------------------------------------------------------------------
    virtual nlohmann::json run(int max_cycles = 100000) = 0;

    // -----------------------------------------------------------------------
    // load_program() — write hex text into memory and reset state.
    // -----------------------------------------------------------------------
    virtual bool load_program(const std::string& hex_str) = 0;

    // -----------------------------------------------------------------------
    // reset() — clear pipeline state, keep memory contents.
    // -----------------------------------------------------------------------
    virtual void reset() = 0;

    // -----------------------------------------------------------------------
    // get_state_json() — snapshot current state without advancing.
    // -----------------------------------------------------------------------
    virtual nlohmann::json get_state_json() const = 0;

    // -----------------------------------------------------------------------
    // configure() — hot-swap config (predictor, cache, forwarding, etc.)
    // -----------------------------------------------------------------------
    virtual void configure(const nlohmann::json& cfg) = 0;

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------
    virtual bool is_halted() const = 0;
    virtual uint64_t cycle_count() const = 0;

    // -----------------------------------------------------------------------
    // Engine identity — "scalar", "superscalar", "vliw"
    // -----------------------------------------------------------------------
    virtual std::string engine_type() const = 0;
};

} // namespace rv32i
