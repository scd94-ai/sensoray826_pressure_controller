cmake_minimum_required(VERSION 3.16)

project(sensoray826_pressure_control VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# The Sensoray SDK provides the 826_64 library. Its public headers are
# included in this repository under include/.
find_library(SENSORAY826_LIB
    NAMES 826_64
    HINTS
        "$ENV{SENSORAY826_SDK_ROOT}"
        "$ENV{HOME}/Downloads/sdk_826_linux_3.3.17/middleware"
    PATH_SUFFIXES middleware
)

if(NOT SENSORAY826_LIB)
    message(FATAL_ERROR
        "Could not find the Sensoray 826 library (826_64). "
        "Set SENSORAY826_SDK_ROOT to the SDK or middleware directory."
    )
endif()

# Make the SDK dependency explicit so it is propagated to consumers of the
# reusable driver library and reconstructed by the installed package config.
add_library(Sensoray826SDK::Sensoray826SDK UNKNOWN IMPORTED)
set_target_properties(Sensoray826SDK::Sensoray826SDK PROPERTIES
    IMPORTED_LOCATION "${SENSORAY826_LIB}"
)

# Reusable driver library.
add_library(sensoray826_driver
    src/sensoray826.cpp
    src/festo.cpp
    src/potentiometer.cpp
    src/pressure_sensor.cpp
)
add_library(sensoray826::driver ALIAS sensoray826_driver)
set_target_properties(sensoray826_driver PROPERTIES
    EXPORT_NAME driver
)

target_include_directories(sensoray826_driver PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(sensoray826_driver PUBLIC
    Sensoray826SDK::Sensoray826SDK
)

# Keep the original standalone controller application.
add_executable(sensoray826_pressure_control
    src/main.cpp
)
target_link_libraries(sensoray826_pressure_control PRIVATE
    sensoray826_driver
)

# Install the library, headers, and standalone application.
install(TARGETS sensoray826_driver
    EXPORT sensoray826_driverTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(TARGETS sensoray826_pressure_control
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Export sensoray826::driver for downstream CMake projects.
install(EXPORT sensoray826_driverTargets
    FILE sensoray826_driverTargets.cmake
    NAMESPACE sensoray826::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sensoray826_driver
)

configure_package_config_file(
    cmake/sensoray826_driverConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/sensoray826_driverConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sensoray826_driver
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/sensoray826_driverConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/sensoray826_driverConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/sensoray826_driverConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sensoray826_driver
)
