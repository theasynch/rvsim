// cache.cpp
#include "cache/cache.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace rv32i {

int Cache::log2i(int n) {
    int r = 0;
    while (n > 1) { n >>= 1; ++r; }
    return r;
}

Cache::Cache(CacheConfig cfg) : cfg_(cfg) {
    // Fully-associative: associativity = total lines = size / block_size
    if (cfg_.associativity == 0)
        cfg_.associativity = cfg_.size_bytes / cfg_.block_size;

    int total_lines = cfg_.size_bytes / cfg_.block_size;
    num_sets_       = total_lines / cfg_.associativity;
    offset_bits_    = log2i(cfg_.block_size);
    index_bits_     = (num_sets_ > 1) ? log2i(num_sets_) : 0;

    // Initialize: num_sets_ sets, each with cfg_.associativity ways
    sets_.assign(num_sets_, std::vector<CacheLine>(cfg_.associativity));
}

uint32_t Cache::get_offset(uint32_t addr) const {
    return addr & ((1u << offset_bits_) - 1);
}

uint32_t Cache::get_index(uint32_t addr) const {
    if (index_bits_ == 0) return 0;
    return (addr >> offset_bits_) & ((1u << index_bits_) - 1);
}

uint32_t Cache::get_tag(uint32_t addr) const {
    return addr >> (offset_bits_ + index_bits_);
}

int Cache::find_line(int set_idx, uint32_t tag) const {
    for (int w = 0; w < cfg_.associativity; ++w) {
        if (sets_[set_idx][w].valid && sets_[set_idx][w].tag == tag)
            return w;
    }
    return -1; // miss
}

int Cache::choose_victim(int set_idx) const {
    // First: find an invalid (empty) line
    for (int w = 0; w < cfg_.associativity; ++w) {
        if (!sets_[set_idx][w].valid) return w;
    }
    // Eviction based on replacement policy
    if (cfg_.policy == ReplacementPolicy::LRU) {
        // Choose the line with the smallest (oldest) LRU counter
        int victim = 0;
        for (int w = 1; w < cfg_.associativity; ++w) {
            if (sets_[set_idx][w].order < sets_[set_idx][victim].order)
                victim = w;
        }
        return victim;
    } else { // FIFO
        // Choose the line with the smallest FIFO order
        int victim = 0;
        for (int w = 1; w < cfg_.associativity; ++w) {
            if (sets_[set_idx][w].order < sets_[set_idx][victim].order)
                victim = w;
        }
        return victim;
    }
}

bool Cache::access(uint32_t address, bool is_write) {
    ++accesses_;
    uint32_t tag      = get_tag(address);
    int      set_idx  = (int)get_index(address);
    int      way      = find_line(set_idx, tag);

    if (way >= 0) {
        // HIT
        ++hits_;
        if (cfg_.policy == ReplacementPolicy::LRU) {
            // Update LRU order: this line becomes the most recently used
            sets_[set_idx][way].order = ++time_ctr_;
        }
        return true;
    }

    // MISS — bring in the block
    ++misses_;
    int victim = choose_victim(set_idx);
    sets_[set_idx][victim].valid = true;
    sets_[set_idx][victim].tag   = tag;
    sets_[set_idx][victim].order = ++time_ctr_;
    return false;
}

void Cache::reset() {
    for (auto& set : sets_)
        for (auto& line : set)
            line = CacheLine{};
    accesses_ = hits_ = misses_ = 0;
    time_ctr_ = 0;
}

nlohmann::json Cache::get_state() const {
    nlohmann::json j;
    j["name"]          = cfg_.name;
    j["size_bytes"]    = cfg_.size_bytes;
    j["block_size"]    = cfg_.block_size;
    j["associativity"] = cfg_.associativity;
    j["num_sets"]      = num_sets_;
    j["policy"]        = (cfg_.policy == ReplacementPolicy::LRU) ? "LRU" : "FIFO";
    j["accesses"]      = accesses_;
    j["hits"]          = hits_;
    j["misses"]        = misses_;
    j["hit_rate"]      = hit_rate();

    // Export first few sets for visualization
    nlohmann::json sets_j = nlohmann::json::array();
    int show_sets = std::min(num_sets_, 16);
    for (int s = 0; s < show_sets; ++s) {
        nlohmann::json set_j = nlohmann::json::array();
        for (int w = 0; w < cfg_.associativity; ++w) {
            const auto& line = sets_[s][w];
            char tag_buf[12];
            snprintf(tag_buf, sizeof(tag_buf), "0x%05X", line.tag);
            set_j.push_back({
                {"valid", line.valid},
                {"tag",   line.valid ? std::string(tag_buf) : "---"},
                {"order", line.order}
            });
        }
        sets_j.push_back(set_j);
    }
    j["sets"] = sets_j;
    return j;
}

} // namespace rv32i
