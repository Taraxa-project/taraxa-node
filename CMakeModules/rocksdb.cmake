# ========================================================================== #
# RocksDB key-value store                                                    #
# ========================================================================== #
include(FetchContent)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

FetchContent_Declare(
  rocksdb
  GIT_REPOSITORY https://github.com/facebook/rocksdb
  GIT_TAG v8.5.3
  GIT_SHALLOW TRUE
)

FetchContent_GetProperties(rocksdb)

message(STATUS "Populating rocksdb")
set(USE_RTTI 1)
set(WITH_LZ4 ON)
set(WITH_GFLAGS OFF)
set(FAIL_ON_WARNINGS OFF)
set(WITH_TESTS OFF CACHE INTERNAL "")
set(WITH_JNI OFF CACHE INTERNAL "")
set(WITH_TOOLS OFF CACHE INTERNAL "")
set(WITH_BENCHMARK_TOOLS  OFF CACHE INTERNAL "")
set(WITH_CORE_TOOLS  OFF CACHE INTERNAL "")
set(WITH_TRACE_TOOLS  OFF CACHE INTERNAL "")
set(ROCKSDB_BUILD_SHARED ${BUILD_SHARED_LIBS} CACHE INTERNAL "")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -Wno-error=unused-const-variable -Wno-error=pedantic -Wno-error=format-pedantic -Wno-error=gnu-zero-variadic-macro-arguments -Wno-error=extra-semi -Wc++98-compat-extra-semi -Wno-error=deprecated-copy -Wno-deprecated-copy -Wno-error=pessimizing-move -Wno-pessimizing-move -Wno-pessimizing-move")

FetchContent_MakeAvailable(rocksdb)
