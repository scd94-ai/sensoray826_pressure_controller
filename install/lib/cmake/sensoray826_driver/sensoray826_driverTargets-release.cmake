#----------------------------------------------------------------
# Generated CMake target import file for configuration "RELEASE".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "sensoray826::driver" for configuration "RELEASE"
set_property(TARGET sensoray826::driver APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sensoray826::driver PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsensoray826_driver.a"
  )

list(APPEND _cmake_import_check_targets sensoray826::driver )
list(APPEND _cmake_import_check_files_for_sensoray826::driver "${_IMPORT_PREFIX}/lib/libsensoray826_driver.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
