// memory.cpp
#include "core/memory.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <nlohmann/json.hpp>

namespace rv32i {

Memory::Memory() : data_(MEM_SIZE, 0) {}

void Memory::reset() {
    std::fill(data_.begin(), data_.end(), 0);
}

uint8_t Memory::read_byte(uint32_t addr) const {
    if (addr >= MEM_SIZE) return 0;
    return data_[addr];
}

void Memory::write_byte(uint32_t addr, uint8_t val) {
    if (addr >= MEM_SIZE) return;
    data_[addr] = val;
}

// fetch32: little-endian 32-bit word load (RISC-V is LE)
uint32_t Memory::fetch32(uint32_t addr) const {
    if (addr + 3 >= MEM_SIZE) return 0;
    uint32_t w = 0;
    w |= (uint32_t)data_[addr + 0] << 0;
    w |= (uint32_t)data_[addr + 1] << 8;
    w |= (uint32_t)data_[addr + 2] << 16;
    w |= (uint32_t)data_[addr + 3] << 24;
    return w;
}

int32_t Memory::load(uint32_t addr, MemSize size, bool sign_extend) const {
    if (addr >= MEM_SIZE) return 0;
    switch (size) {
        case MemSize::BYTE: {
            uint8_t b = data_[addr];
            if (sign_extend) return (int8_t)b;
            return (uint32_t)b;
        }
        case MemSize::HALFWORD: {
            if (addr + 1 >= MEM_SIZE) return 0;
            uint16_t h = (uint16_t)data_[addr] | ((uint16_t)data_[addr+1] << 8);
            if (sign_extend) return (int16_t)h;
            return (uint32_t)h;
        }
        case MemSize::WORD:
        default: {
            return (int32_t)fetch32(addr);
        }
    }
}

void Memory::store(uint32_t addr, int32_t value, MemSize size) {
    if (addr >= MEM_SIZE) return;
    switch (size) {
        case MemSize::BYTE:
            data_[addr] = (uint8_t)(value & 0xFF);
            break;
        case MemSize::HALFWORD:
            if (addr + 1 < MEM_SIZE) {
                data_[addr]   = (uint8_t)(value & 0xFF);
                data_[addr+1] = (uint8_t)((value >> 8) & 0xFF);
            }
            break;
        case MemSize::WORD:
        default:
            if (addr + 3 < MEM_SIZE) {
                data_[addr]   = (uint8_t)(value & 0xFF);
                data_[addr+1] = (uint8_t)((value >> 8)  & 0xFF);
                data_[addr+2] = (uint8_t)((value >> 16) & 0xFF);
                data_[addr+3] = (uint8_t)((value >> 24) & 0xFF);
            }
            break;
    }
}

// ---------------------------------------------------------------------------
// load_hex: parses a text file where each line is 8 hex characters (1 word)
// Words are stored in little-endian order at TEXT_BASE.
// ---------------------------------------------------------------------------
bool Memory::load_hex(const std::string& hex_str) {
    reset();
    std::istringstream ss(hex_str);
    std::string line;
    uint32_t addr = TEXT_BASE;

    while (std::getline(ss, line)) {
        // Strip whitespace and comments (# ...)
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        while (!line.empty() && (line.back() == ' ' || line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        while (!line.empty() && line.front() == ' ')
            line = line.substr(1);

        if (line.empty()) continue;

        // Each line should be exactly 8 hex digits (32-bit word)
        if (line.size() != 8) return false;

        try {
            uint32_t word = (uint32_t)std::stoul(line, nullptr, 16);
            store(addr, (int32_t)word, MemSize::WORD);
            addr += 4;
        } catch (...) {
            return false;
        }
    }
    return true;
}

bool Memory::load_from_bytes(const std::vector<uint8_t>& bytes, uint32_t base_addr) {
    reset();
    for (size_t i = 0; i < bytes.size() && (base_addr + i) < MEM_SIZE; ++i) {
        data_[base_addr + i] = bytes[i];
    }
    return true;
}

nlohmann::json Memory::dump_range(uint32_t start, uint32_t length) const {
    nlohmann::json j = nlohmann::json::array();
    for (uint32_t i = start; i < start + length && i < MEM_SIZE; i += 4) {
        char addr_buf[12], val_buf[12];
        snprintf(addr_buf, sizeof(addr_buf), "0x%08X", i);
        uint32_t word = fetch32(i);
        snprintf(val_buf, sizeof(val_buf), "0x%08X", word);
        j.push_back({{"addr", addr_buf}, {"value", val_buf}});
    }
    return j;
}

} // namespace rv32i
