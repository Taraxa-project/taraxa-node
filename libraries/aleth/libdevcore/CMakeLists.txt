file(GLOB sources "*.cpp")
file(GLOB headers "*.h")

add_library(devcore ${sources} ${headers})

target_link_libraries(devcore PUBLIC CONAN_PKG::boost CONAN_PKG::jsoncpp ethash)
target_include_directories(devcore SYSTEM PUBLIC ../)
