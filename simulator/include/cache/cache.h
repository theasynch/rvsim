// =============================================================================
// cache.h — Configurable Cache (L1/L2, Direct-Mapped / Set-Assoc / Fully-Assoc)
// =============================================================================
//
// Cache parameters:
//   - size_bytes:     total cache size (e.g. 8192 = 8KB)
//   - block_size:     bytes per cache line (e.g. 64 bytes)
//   - associativity:  1 = direct-mapped, N = N-way, 0 = fully-associative
//   - policy:         LRU or FIFO replacement
//
// Address breakdown:
//   [  TAG  |  INDEX  |  BLOCK OFFSET  ]
//
//   offset_bits = log2(block_size)
//   index_bits  = log2(num_sets)   where num_sets = size / (block_size * assoc)
//   tag_bits    = 32 - offset_bits - index_bits
// =============================================================================
#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

namespace rv32i {

enum class ReplacementPolicy { LRU, FIFO };

struct CacheLine {
    bool     valid    = false;
    uint32_t tag      = 0;
    int      order    = 0;  // Used by FIFO/LRU ordering
};

struct CacheConfig {
    int size_bytes    = 8 * 1024; // 8KB default
    int block_size    = 64;       // 64-byte cache lines
    int associativity = 4;        // 4-way set associative
    ReplacementPolicy policy = ReplacementPolicy::LRU;
    std::string name = "L1";
};

class Cache {
public:
    explicit Cache(CacheConfig cfg);

    // access() — simulate a cache access (read or write).
    // Returns true on hit, false on miss.
    // address: byte address to access
    // is_write: true for stores, false for loads
    bool access(uint32_t address, bool is_write = false);

    // Stats
    uint64_t accesses() const { return accesses_; }
    uint64_t hits()     const { return hits_;     }
    uint64_t misses()   const { return misses_;   }
    double   hit_rate() const {
        return accesses_ > 0 ? (double)hits_ / accesses_ : 0.0;
    }

    void reset();
    nlohmann::json get_state() const;  // Returns cache contents for visualization
    const CacheConfig& config() const { return cfg_; }

    // Address decomposition helpers (for visualization)
    uint32_t get_tag   (uint32_t addr) const;
    uint32_t get_index (uint32_t addr) const;
    uint32_t get_offset(uint32_t addr) const;

private:
    CacheConfig cfg_;
    int num_sets_;
    int offset_bits_;
    int index_bits_;

    // sets_[set_index][way] = CacheLine
    std::vector<std::vector<CacheLine>> sets_;

    uint64_t accesses_ = 0;
    uint64_t hits_     = 0;
    uint64_t misses_   = 0;
    int      time_ctr_ = 0; // Global counter for LRU ordering

    // Find a line in a set (returns way index or -1)
    int find_line(int set_idx, uint32_t tag) const;

    // Choose a victim way for eviction
    int choose_victim(int set_idx) const;

    static int log2i(int n);
};

} // namespace rv32i
