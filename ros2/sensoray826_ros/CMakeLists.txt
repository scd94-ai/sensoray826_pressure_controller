cmake_minimum_required(VERSION 3.16)
project(sensoray826_ros LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(soft_actuator_msgs REQUIRED)

# Standalone hardware sources stay in their original repository.
get_filename_component(CONTROLLER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
find_path(S826_API_INCLUDE_DIR 826api.h
  PATHS "$ENV{HOME}/Downloads/sdk_826_linux_3.3.17/middleware"
        "$ENV{HOME}/Downloads/sdk_826_linux_3.3.17"
  PATH_SUFFIXES include .)
find_library(S826_LIBRARY NAMES 826_64
  PATHS "$ENV{HOME}/Downloads/sdk_826_linux_3.3.17/middleware")
if(NOT S826_API_INCLUDE_DIR OR NOT S826_LIBRARY)
  message(FATAL_ERROR "Sensoray 826 SDK not found. Set S826_API_INCLUDE_DIR and S826_LIBRARY.")
endif()

add_executable(sensoray_node
  src/sensoray_node.cpp
  "${CONTROLLER_ROOT}/src/sensoray826.cpp"
  "${CONTROLLER_ROOT}/src/festo.cpp"
)
target_include_directories(sensoray_node PRIVATE
  "${CONTROLLER_ROOT}/include" "${S826_API_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/include")
ament_target_dependencies(sensoray_node rclcpp soft_actuator_msgs)
target_link_libraries(sensoray_node "${S826_LIBRARY}")
install(TARGETS sensoray_node DESTINATION lib/${PROJECT_NAME})
ament_package()
