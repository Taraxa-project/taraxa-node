#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "ethash::ethash" for configuration "Release"
set_property(TARGET ethash::ethash APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(ethash::ethash PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C;CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libethash.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS ethash::ethash )
list(APPEND _IMPORT_CHECK_FILES_FOR_ethash::ethash "${_IMPORT_PREFIX}/lib/libethash.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
