// =============================================================================
// register_file.h — RISC-V Register File
// =============================================================================
//
// The register file holds the 32 general-purpose integer registers (x0–x31).
// Hardware fact: x0 (zero) is hardwired to 0 — writes to it are discarded.
//
// In a real pipeline, the register file has TWO read ports and ONE write port,
// allowing simultaneous reads in ID stage and write in WB stage.
// =============================================================================
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

namespace rv32i {

class RegisterFile {
public:
    RegisterFile();

    // Read register value. x0 always returns 0.
    int32_t read(uint8_t idx) const;

    // Write register value. Writes to x0 are silently discarded.
    void    write(uint8_t idx, int32_t value);

    // Reset all registers to 0
    void    reset();

    // Dump all registers as JSON for the API
    nlohmann::json to_json() const;

    // Get the raw array for direct inspection
    const std::array<int32_t, 32>& all() const { return regs_; }

private:
    std::array<int32_t, 32> regs_{};
};

} // namespace rv32i
