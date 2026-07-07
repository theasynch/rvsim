// simulator/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

add_library(simulator_lib STATIC
    src/isa/decoder.cpp
    src/core/register_file.cpp
    src/core/memory.cpp
    src/core/pipeline.cpp
    src/core/hazard_unit.cpp
    src/core/forwarding_unit.cpp
    src/branch/static_pred.cpp
    src/branch/two_bit_pred.cpp
    src/branch/gshare_pred.cpp
    src/branch/tournament_pred.cpp
    src/cache/cache.cpp
)

target_include_directories(simulator_lib
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(simulator_lib PUBLIC cxx_std_17)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(simulator_lib PUBLIC nlohmann_json::nlohmann_json)
