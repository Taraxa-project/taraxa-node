set(HEADERS
    include/common/constants.hpp
    include/common/static_init.hpp
    include/common/types.hpp
)

set(SOURCES
    src/constants.cpp
    src/types.cpp
)

add_library(common ${SOURCES} ${HEADERS})
target_include_directories(common PUBLIC "include")
target_link_libraries(common PRIVATE util aleth)