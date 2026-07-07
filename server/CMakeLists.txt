cmake_minimum_required(VERSION 3.20)

add_library(server_lib STATIC
    src/api_server.cpp
)

target_include_directories(server_lib
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/third_party"        # resolves <nlohmann/json.hpp>
        "${CMAKE_SOURCE_DIR}/third_party/httplib" # resolves <httplib.h>
        "${CMAKE_SOURCE_DIR}/simulator/include"
)

target_link_libraries(server_lib
    PUBLIC
        simulator_lib
)

if(WIN32)
    target_link_libraries(server_lib PUBLIC ws2_32)
endif()
