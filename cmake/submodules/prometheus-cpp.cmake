include_guard()

option(BUILD_TESTING OFF)
option(BUILD_SHARED_LIBS OFF)
option(ENABLE_TESTING OFF)
add_subdirectory(submodules/prometheus-cpp EXCLUDE_FROM_ALL)