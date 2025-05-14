set(HEADERS
    include/spdlogger/logging.hpp
#    include/spdlogger/logger_config.hpp
)

set(SOURCES
    src/logging.cpp
#    src/logger_config.cpp
)

find_package(spdlog REQUIRED)
add_library(spdlogger ${SOURCES} ${HEADERS})
target_include_directories(spdlogger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(spdlogger PUBLIC spdlog::spdlog common)

install(TARGETS spdlogger
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)