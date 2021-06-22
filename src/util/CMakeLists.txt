set(HEADERS
    include/util/default_construct_copyable_movable.hpp
    include/util/encoding_rlp.hpp
    include/util/exit_stack.hpp
    include/util/functional.hpp
    include/util/jsoncpp.hpp
    include/util/lazy.hpp
    include/util/range_view.hpp
    include/util/thread_pool.hpp
    include/util/util.hpp
)

set(SOURCES
    src/jsoncpp.cpp
    src/thread_pool.cpp
    src/util.cpp
)

add_library(util ${SOURCES} ${HEADERS})
target_include_directories(util PUBLIC "include")
target_link_libraries(util PRIVATE 
    common
    aleth
    CONAN_PKG::jsoncpp
    CONAN_PKG::boost
)