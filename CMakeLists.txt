cmake_minimum_required(VERSION 3.14)
project(icom_udp_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ICOM_UDP_PORTAUDIO "Play RX audio through PortAudio" OFF)

if(ICOM_UDP_PORTAUDIO)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PORTAUDIO QUIET portaudio-2.0)
    endif()
    if(NOT PORTAUDIO_FOUND)
        find_path(PORTAUDIO_INCLUDE_DIRS portaudio.h
            HINTS /opt/homebrew/include /usr/local/include)
        find_library(PORTAUDIO_LIBRARIES NAMES portaudio
            HINTS /opt/homebrew/lib /usr/local/lib)
    endif()
    if(NOT PORTAUDIO_INCLUDE_DIRS OR NOT PORTAUDIO_LIBRARIES)
        message(FATAL_ERROR
            "ICOM_UDP_PORTAUDIO=ON but PortAudio was not found. "
            "Install portaudio (e.g. brew install portaudio) or disable the option.")
    endif()
    set(ICOM_AUDIO_SOURCE audio_output_portaudio.cpp)
else()
    set(ICOM_AUDIO_SOURCE audio_output_stub.cpp)
endif()

function(icom_link_portaudio target)
    target_compile_definitions(${target} PRIVATE ICOM_UDP_PORTAUDIO=1)
    target_include_directories(${target} PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE ${PORTAUDIO_LIBRARIES})
    if(APPLE)
        target_link_libraries(${target} PRIVATE
            "-framework CoreAudio" "-framework AudioToolbox" "-framework AudioUnit"
            "-framework CoreFoundation" "-framework CoreServices")
    endif()
endfunction()

add_executable(icom_udp_example main.cpp ${ICOM_AUDIO_SOURCE})
target_include_directories(icom_udp_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(ICOM_UDP_PORTAUDIO)
    icom_link_portaudio(icom_udp_example)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(icom_udp_example PRIVATE pthread)
endif()

enable_testing()

add_executable(audio_output_test tests/audio_output_test.cpp ${ICOM_AUDIO_SOURCE})
target_include_directories(audio_output_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(audio_output_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
if(ICOM_UDP_PORTAUDIO)
    icom_link_portaudio(audio_output_test)
endif()

add_test(NAME audio_output_test COMMAND audio_output_test)
