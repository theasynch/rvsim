cmake_minimum_required(VERSION 3.20)

add_library(simulator_lib STATIC
    src/isa/decoder.cpp
    src/core/register_file.cpp
    src/core/memory.cpp
    src/core/hazard_unit.cpp
    src/core/forwarding_unit.cpp
    src/core/pipeline.cpp
    src/branch/static_pred.cpp
    src/branch/two_bit_pred.cpp
    src/branch/gshare_pred.cpp
    src/branch/tournament_pred.cpp
    src/cache/cache.cpp
)

target_include_directories(simulator_lib
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/third_party"   # resolves <nlohmann/json.hpp>
)
