set(HEADERS
    include/config/config.hpp
    include/config/config_exception.hpp

    include/config/pbft.hpp
    include/config/chain.hpp
)

set(SOURCES
    src/config.cpp
    src/pbft.cpp
    src/chain.cpp
)

add_library(config ${SOURCES} ${HEADERS})
target_include_directories(config PUBLIC "include")
target_link_libraries(config PRIVATE app_base)
