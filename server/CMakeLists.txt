cmake_minimum_required(VERSION 3.20)

add_library(server_lib STATIC
    src/api_server.cpp
    src/rtl_pipeline.cpp
    src/rtl/Vrv32i_top.cpp
    src/rtl/Vrv32i_top__Syms.cpp
    src/rtl/Vrv32i_top__Trace__0.cpp
    src/rtl/Vrv32i_top__Trace__0__Slow.cpp
    src/rtl/Vrv32i_top__TraceDecls__0__Slow.cpp
    src/rtl/Vrv32i_top___024root__DepSet_h6be8b632__0.cpp
    src/rtl/Vrv32i_top___024root__DepSet_h6be8b632__0__Slow.cpp
    src/rtl/Vrv32i_top___024root__DepSet_hbb1bc11c__0.cpp
    src/rtl/Vrv32i_top___024root__DepSet_hbb1bc11c__0__Slow.cpp
    src/rtl/Vrv32i_top___024root__Slow.cpp
    "${CMAKE_SOURCE_DIR}/third_party/verilator/include/verilated.cpp"
    "${CMAKE_SOURCE_DIR}/third_party/verilator/include/verilated_vcd_c.cpp"
    "${CMAKE_SOURCE_DIR}/third_party/verilator/include/verilated_threads.cpp"
)

target_include_directories(server_lib
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/third_party"        # resolves <nlohmann/json.hpp>
        "${CMAKE_SOURCE_DIR}/third_party/httplib" # resolves <httplib.h>
        "${CMAKE_SOURCE_DIR}/simulator/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rtl"
        "${CMAKE_SOURCE_DIR}/third_party/verilator/include"
        "${CMAKE_SOURCE_DIR}/third_party/verilator/include/vltstd"
)

target_link_libraries(server_lib
    PUBLIC
        simulator_lib
)

if(WIN32)
    target_link_libraries(server_lib PUBLIC ws2_32)
endif()
