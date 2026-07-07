cmake_minimum_required(VERSION 3.20)

# ============================================================
# RVSim — RISC-V Pipeline & Memory Hierarchy Simulator
# ============================================================
project(RVSim VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Static runtime linking: makes the .exe standalone on Windows
# (no VCRUNTIME or MSVCP DLLs needed by the end user)
# ------------------------------------------------------------
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ------------------------------------------------------------
# vcpkg toolchain — pulled in via CMAKE_TOOLCHAIN_FILE.
# If you're using the vcpkg manifest (vcpkg.json in root),
# vcpkg auto-installs dependencies on first cmake configure.
# ------------------------------------------------------------

# ------------------------------------------------------------
# Find packages installed by vcpkg
# ------------------------------------------------------------
find_package(httplib CONFIG REQUIRED)      # cpp-httplib HTTP server
find_package(nlohmann_json CONFIG REQUIRED) # JSON serialization

# ------------------------------------------------------------
# Source tree
# ------------------------------------------------------------
add_subdirectory(simulator)
add_subdirectory(server)

# ------------------------------------------------------------
# Frontend embedding step:
# Runs embed_frontend.py to bake HTML/CSS/JS into a C++ header.
# This runs every time CMake configures, so changes to the
# frontend are picked up automatically.
# ------------------------------------------------------------
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(FRONTEND_EMBED_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/server/include/embedded_frontend.h")
set(FRONTEND_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/frontend/index.html")

add_custom_command(
    OUTPUT "${FRONTEND_EMBED_OUTPUT}"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_frontend.py"
            "${FRONTEND_INPUT}"
            "${FRONTEND_EMBED_OUTPUT}"
    DEPENDS "${FRONTEND_INPUT}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_frontend.py"
    COMMENT "Embedding frontend into C++ header..."
)

add_custom_target(EmbedFrontend ALL DEPENDS "${FRONTEND_EMBED_OUTPUT}")

# ------------------------------------------------------------
# Main executable
# ------------------------------------------------------------
add_executable(RVSim main.cpp)

add_dependencies(RVSim EmbedFrontend)

target_link_libraries(RVSim
    PRIVATE
        simulator_lib
        server_lib
        httplib::httplib
        nlohmann_json::nlohmann_json
)

target_include_directories(RVSim
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/simulator/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/server/include"
)

# Windows: link ws2_32 (Winsock) needed by cpp-httplib
if(WIN32)
    target_link_libraries(RVSim PRIVATE ws2_32)
endif()

# ------------------------------------------------------------
# Output directory — works for both single and multi-config
# ------------------------------------------------------------
set_target_properties(RVSim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/bin/Debug"
)

message(STATUS "RVSim configured. Build with: cmake --build build --config Release")
