set(HEADERS
    include/logger/log.hpp
    include/logger/log_config.hpp
)

set(SOURCES
    src/log.cpp
    src/log_config.cpp
)

add_library(logger STATIC ${SOURCES} ${HEADERS})
target_include_directories(logger PUBLIC "include")
target_link_libraries(logger PRIVATE config common aleth CONAN_PKG::boost)