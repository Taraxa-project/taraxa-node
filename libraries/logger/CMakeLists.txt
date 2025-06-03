set(HEADERS
    include/logger/logging.hpp
    include/logger/logging_config.hpp
)

set(SOURCES
    src/logging.cpp
)

find_package(spdlog REQUIRED)
add_library(logger ${SOURCES} ${HEADERS})
target_include_directories(logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(logger PUBLIC spdlog::spdlog)

install(TARGETS logger
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)