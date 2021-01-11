add_executable(taraxad main.cpp)
target_link_libraries(taraxad app_base)

install(PROGRAMS ${CMAKE_BINARY_DIR}/src/taraxad/taraxad DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT taraxad)
