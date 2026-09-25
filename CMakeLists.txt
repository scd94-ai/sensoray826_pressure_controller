set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

cmake_minimum_required(VERSION 3.16)

project(sensoray826_pressure_control LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sensoray826_pressure_control
    src/main.cpp
    src/Festo.cpp
)

target_include_directories(sensoray826_pressure_control
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

find_library(SENSORAY826_LIB
    NAMES 826_64
    PATHS
        $ENV{HOME}/Downloads/sdk_826_linux_3.3.17/middleware
)

if(NOT SENSORAY826_LIB)
    message(FATAL_ERROR "Could not find the Sensoray 826 library.")
endif()

target_link_libraries(sensoray826_pressure_control
    PRIVATE
        ${SENSORAY826_LIB}
)


