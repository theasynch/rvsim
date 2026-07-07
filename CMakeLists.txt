cmake_minimum_required(VERSION 3.20)
project(RVSim VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------
# Third-party headers (bundled — no vcpkg / find_package needed)
# ------------------------------------------------------------
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")

# cpp-httplib (single header)
add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE "${THIRD_PARTY_DIR}/httplib")
# httplib uses WinSock on Windows — link it in
if(WIN32)
    target_link_libraries(httplib INTERFACE ws2_32)
endif()

# nlohmann/json (single header)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE "${THIRD_PARTY_DIR}/nlohmann")

# ------------------------------------------------------------
# Static runtime on Windows — zero DLL dependencies
# ------------------------------------------------------------
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W3 /wd4996)
endif()

# ------------------------------------------------------------
# Simulator library
# ------------------------------------------------------------
add_subdirectory(simulator)

# ------------------------------------------------------------
# Server library
# ------------------------------------------------------------
add_subdirectory(server)

# ------------------------------------------------------------
# Frontend embedding (bake index.html into a C++ header)
# ------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(FRONTEND_HTML   "${CMAKE_SOURCE_DIR}/frontend/index.html")
set(EMBEDDED_HEADER "${CMAKE_SOURCE_DIR}/server/include/embedded_frontend.h")

add_custom_command(
    OUTPUT  ${EMBEDDED_HEADER}
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/embed_frontend.py"
            "${FRONTEND_HTML}"
            "${EMBEDDED_HEADER}"
    DEPENDS ${FRONTEND_HTML}
            "${CMAKE_SOURCE_DIR}/tools/embed_frontend.py"
    COMMENT "Embedding frontend/index.html into C++ header..."
)

add_custom_target(embed_frontend ALL DEPENDS ${EMBEDDED_HEADER})

# ------------------------------------------------------------
# Main executable
# ------------------------------------------------------------
add_executable(RVSim main.cpp)

add_dependencies(RVSim embed_frontend)

target_include_directories(RVSim PRIVATE
    "${CMAKE_SOURCE_DIR}/simulator/include"
    "${CMAKE_SOURCE_DIR}/server/include"
    "${THIRD_PARTY_DIR}/httplib"
    "${THIRD_PARTY_DIR}/nlohmann"
)

target_link_libraries(RVSim PRIVATE
    simulator_lib
    server_lib
)

if(WIN32)
    target_link_libraries(RVSim PRIVATE ws2_32)
    # Open browser on launch — needs ShellExecute
    target_link_libraries(RVSim PRIVATE shell32)
endif()

# ------------------------------------------------------------
# Output directories
# ------------------------------------------------------------
set_target_properties(RVSim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/bin/Debug"
)

message(STATUS "RVSim configured. Build with: cmake --build build --config Release")
