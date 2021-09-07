
set(HEADERS include/vdf/sortition.hpp include/vdf/config.hpp)
set(SOURCES src/sortition.cpp src/config.cpp)

add_library(vdf ${SOURCES} ${HEADERS})
target_include_directories(vdf PUBLIC "include")
target_link_libraries(vdf PUBLIC 
    common
    aleth
    logger
    taraxa-vdf
    CONAN_PKG::mpfr
    CONAN_PKG::jsoncpp
)