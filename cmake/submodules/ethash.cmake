include_guard()

option(BUILD_TESTING OFF)
option(BUILD_SHARED_LIBS OFF)
option(ETHASH_BUILD_TESTS OFF)
option(HUNTER_ENABLED OFF)
add_subdirectory(submodules/ethash EXCLUDE_FROM_ALL)