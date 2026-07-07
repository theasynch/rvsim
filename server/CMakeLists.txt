cmake_minimum_required(VERSION 3.20)

add_library(server_lib STATIC
    src/api_server.cpp
)

target_include_directories(server_lib
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(server_lib
    PUBLIC
        simulator_lib
        httplib::httplib
        nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(server_lib PUBLIC ws2_32)
endif()
