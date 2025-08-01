# ########## liboqs ###########
include(FetchContent)

FetchContent_Declare(
  liboqs
  GIT_REPOSITORY https://github.com/open-quantum-safe/liboqs.git
  GIT_TAG 0.14.0
  GIT_SHALLOW TRUE
)
set(OQS_BUILD_ONLY_LIB ON CACHE BOOL "" FORCE)
set(OQS_ALGS_ENABLED STD CACHE STRING "" FORCE)
set(OQS_ENABLE_SIG_SPHINCS OFF CACHE BOOL "" FORCE)
set(OQS_ENABLE_KEM_ML_KEM OFF CACHE BOOL "" FORCE)
set(OQS_ENABLE_SIG_ML_DSA OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(liboqs)

# #####################################