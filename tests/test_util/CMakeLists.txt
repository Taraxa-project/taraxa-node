set(HEADERS
  include/test_util/node_dag_creation_fixture.hpp
  include/test_util/samples.hpp
  include/test_util/test_util.hpp
)

set(SOURCES
    src/node_dag_creation_fixture.cpp
    src/samples.cpp
    src/test_util.cpp
)

# Configure file with version
add_library(test_util ${SOURCES} ${HEADERS})
target_include_directories(test_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(test_util PUBLIC core_libs CONAN_PKG::gtest)

install(TARGETS common
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

