// register_file.cpp
#include "core/register_file.h"
#include <cstdio>
#include <stdexcept>

namespace rv32i {

static const char* ABI_NAMES[32] = {
    "zero","ra","sp","gp","tp","t0","t1","t2",
    "s0",  "s1","a0","a1","a2","a3","a4","a5",
    "a6",  "a7","s2","s3","s4","s5","s6","s7",
    "s8",  "s9","s10","s11","t3","t4","t5","t6"
};

RegisterFile::RegisterFile() { reset(); }

void RegisterFile::reset() { regs_.fill(0); }

int32_t RegisterFile::read(uint8_t idx) const {
    if (idx >= 32) return 0;
    return regs_[idx]; // x0 is always 0 because we never write to it
}

void RegisterFile::write(uint8_t idx, int32_t value) {
    if (idx == 0 || idx >= 32) return; // x0 is hardwired to 0
    regs_[idx] = value;
}

nlohmann::json RegisterFile::to_json() const {
    nlohmann::json j = nlohmann::json::array();
    for (int i = 0; i < 32; ++i) {
        j.push_back({
            {"index",   i},
            {"name",    ABI_NAMES[i]},
            {"value",   regs_[i]},
            {"hex",     [&]() {
                char buf[12];
                snprintf(buf, sizeof(buf), "0x%08X", (uint32_t)regs_[i]);
                return std::string(buf);
            }()}
        });
    }
    return j;
}

} // namespace rv32i
