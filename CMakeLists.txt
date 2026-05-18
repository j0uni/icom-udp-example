cmake_minimum_required(VERSION 3.14)
project(icom_udp_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(icom_udp_example main.cpp)
target_include_directories(icom_udp_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(UNIX AND NOT APPLE)
    target_link_libraries(icom_udp_example PRIVATE pthread)
endif()
