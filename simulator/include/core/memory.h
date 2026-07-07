// =============================================================================
// memory.h — Flat Byte-Addressable Memory Model
// =============================================================================
//
// Models a simplified unified memory with separate I-cache and D-cache
// simulation. The raw memory is a flat byte array. Cache behaviour is
// simulated by the cache module — this is the backing store.
//
// Memory map:
//   0x00000000 .. 0x00FFFFFF  — 16MB flat (text + data + stack)
//   Stack grows DOWN from 0x00FFFF00
//   Text starts at  0x00000000
// =============================================================================
#pragma once

#include "isa/rv32i_types.h"
#include <vector>
#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace rv32i {

constexpr uint32_t MEM_SIZE    = 16 * 1024 * 1024; // 16 MB
constexpr uint32_t STACK_BASE  = 0x00FFFF00u;       // Initial SP
constexpr uint32_t TEXT_BASE   = 0x00000000u;       // Program load address

class Memory {
public:
    Memory();

    // -----------------------------------------------------------------------
    // Instruction fetch — always 32-bit, always aligned
    // -----------------------------------------------------------------------
    uint32_t fetch32(uint32_t addr) const;

    // -----------------------------------------------------------------------
    // Data loads — size/signedness controlled by mem_size and signed_ext
    // -----------------------------------------------------------------------
    int32_t  load(uint32_t addr, MemSize size, bool sign_extend) const;

    // -----------------------------------------------------------------------
    // Data stores
    // -----------------------------------------------------------------------
    void     store(uint32_t addr, int32_t value, MemSize size);

    // -----------------------------------------------------------------------
    // Program loading:
    //   load_hex()  — load a simple hex dump (8 hex chars per line = 1 word)
    //   load_raw()  — load raw bytes from a vector
    // -----------------------------------------------------------------------
    bool load_hex(const std::string& hex_str); // Returns false on parse error
    bool load_from_bytes(const std::vector<uint8_t>& bytes, uint32_t base_addr = TEXT_BASE);

    // Reset memory to all zeros
    void reset();

    // Get a slice of memory as JSON (for visualization)
    nlohmann::json dump_range(uint32_t start, uint32_t length) const;

    // Access raw byte (for cache simulation)
    uint8_t  read_byte (uint32_t addr) const;
    void     write_byte(uint32_t addr, uint8_t  val);

    bool in_bounds(uint32_t addr) const { return addr < MEM_SIZE; }

private:
    std::vector<uint8_t> data_;
};

} // namespace rv32i
