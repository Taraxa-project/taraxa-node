include_guard()

option(BUILD_TESTING OFF)
option(BUILD_SHARED_LIBS OFF)
option(WITH_PROCPS OFF)
add_subdirectory(submodules/libff EXCLUDE_FROM_ALL)