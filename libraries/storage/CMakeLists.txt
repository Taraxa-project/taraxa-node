set(HEADERS
    include/storage/storage.hpp
)

set(SOURCES
    src/storage.cpp
)

add_library(storage ${SOURCES} ${HEADERS})
target_include_directories(storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(storage PUBLIC CONAN_PKG::rocksdb)
target_link_libraries(storage PRIVATE 
    common
    config
    logger
    pbft_block
    vote
)

install(TARGETS storage
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)