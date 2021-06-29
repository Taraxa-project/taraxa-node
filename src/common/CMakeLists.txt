set(HEADERS
    include/common/constants.hpp
    include/common/static_init.hpp
    include/common/types.hpp

    include/common/default_construct_copyable_movable.hpp
    include/common/encoding_rlp.hpp
    include/common/exit_stack.hpp
    include/common/functional.hpp
    include/common/jsoncpp.hpp
    include/common/lazy.hpp
    include/common/range_view.hpp
    include/common/thread_pool.hpp
    include/common/util.hpp
)

set(SOURCES
    src/constants.cpp
    src/types.cpp

    src/jsoncpp.cpp
    src/thread_pool.cpp
    src/util.cpp
)

add_library(common STATIC ${SOURCES} ${HEADERS})
target_include_directories(common PUBLIC "include")
target_link_libraries(common PRIVATE aleth CONAN_PKG::jsoncpp)