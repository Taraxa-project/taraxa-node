cmake_minimum_required(VERSION 3.13)

project(taraxa-node)

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(submodules)
add_subdirectory(src)
add_subdirectory(tests)
