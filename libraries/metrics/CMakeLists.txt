set(HEADERS
    include/metrics/metrics_group.hpp
    include/metrics/metrics_service.hpp
    include/metrics/network_metrics.hpp
    include/metrics/pbft_metrics.hpp
    include/metrics/transaction_queue_metrics.hpp
)

set(SOURCES
    src/metrics_service.cpp
)

add_library(metrics ${SOURCES} ${HEADERS})
target_include_directories(metrics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(metrics PUBLIC CONAN_PKG::prometheus-cpp)


install(TARGETS metrics
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)