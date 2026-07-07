// =============================================================================
// perf_counters.h — Performance Counters
// =============================================================================
#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>

namespace rv32i {

struct PerfCounters {
    uint64_t cycles          = 0;
    uint64_t instructions    = 0; // retired (completed WB)
    uint64_t stall_cycles    = 0; // cycles wasted due to load-use stalls
    uint64_t flush_cycles    = 0; // cycles wasted due to branch flushes (mispredictions)
    uint64_t branch_count    = 0;
    uint64_t mispredictions  = 0;
    uint64_t cache_accesses  = 0;
    uint64_t cache_hits      = 0;
    uint64_t cache_misses    = 0;
    uint64_t icache_accesses = 0;
    uint64_t icache_hits     = 0;
    uint64_t dcache_accesses = 0;
    uint64_t dcache_hits     = 0;

    double ipc() const {
        return cycles > 0 ? (double)instructions / cycles : 0.0;
    }

    double miss_rate() const {
        return cache_accesses > 0 ? (double)cache_misses / cache_accesses : 0.0;
    }

    double mispred_rate() const {
        return branch_count > 0 ? (double)mispredictions / branch_count : 0.0;
    }

    void reset() { *this = PerfCounters{}; }

    nlohmann::json to_json() const {
        return {
            {"cycles",           cycles},
            {"instructions",     instructions},
            {"ipc",              ipc()},
            {"stall_cycles",     stall_cycles},
            {"flush_cycles",     flush_cycles},
            {"branch_count",     branch_count},
            {"mispredictions",   mispredictions},
            {"mispred_rate",     mispred_rate()},
            {"cache_accesses",   cache_accesses},
            {"cache_hits",       cache_hits},
            {"cache_misses",     cache_misses},
            {"miss_rate",        miss_rate()},
            {"icache_accesses",  icache_accesses},
            {"icache_hits",      icache_hits},
            {"dcache_accesses",  dcache_accesses},
            {"dcache_hits",      dcache_hits}
        };
    }
};

} // namespace rv32i
