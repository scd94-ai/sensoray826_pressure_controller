

####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was sensoray826_driverConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

# Find the Sensoray SDK used by the driver.

find_path(SENSORAY826_INCLUDE_DIR
    NAMES 826api.h
    HINTS
        "$ENV{SENSORAY826_SDK_ROOT}"
        "$ENV{HOME}/Downloads/sdk_826_linux_3.3.17/middleware"
    PATH_SUFFIXES middleware
    REQUIRED
)

find_library(SENSORAY826_LIB
    NAMES 826_64
    HINTS
        "$ENV{SENSORAY826_SDK_ROOT}"
        "$ENV{HOME}/Downloads/sdk_826_linux_3.3.17/middleware"
    PATH_SUFFIXES middleware
    REQUIRED
)

if(NOT TARGET Sensoray826SDK::Sensoray826SDK)
    add_library(Sensoray826SDK::Sensoray826SDK UNKNOWN IMPORTED)

    set_target_properties(Sensoray826SDK::Sensoray826SDK PROPERTIES
        IMPORTED_LOCATION "${SENSORAY826_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${SENSORAY826_INCLUDE_DIR}"
    )
endif()

include("${CMAKE_CURRENT_LIST_DIR}/sensoray826_driverTargets.cmake")

check_required_components(sensoray826_driver)
