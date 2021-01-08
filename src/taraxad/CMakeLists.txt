add_executable(taraxad main.cpp)
target_link_libraries(taraxad app_base)

if(CLANG_TIDY_EXE)
    set_target_properties(taraxad PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}" )
endif()

install(PROGRAMS ${CMAKE_BINARY_DIR}/src/taraxad/taraxad DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT taraxad)
