set(HEADERS
    include/common/constants.hpp
    include/common/static_init.hpp
    include/common/types.hpp
    include/common/config_exception.hpp
    include/common/vrf_wrapper.hpp

    include/common/default_construct_copyable_movable.hpp
    include/common/encoding_rlp.hpp
    include/common/functional.hpp
    include/common/jsoncpp.hpp
    include/common/lazy.hpp
    include/common/range_view.hpp
    include/common/thread_pool.hpp
    include/common/util.hpp
)

set(SOURCES
    src/constants.cpp
    src/jsoncpp.cpp
    src/thread_pool.cpp
    src/util.cpp
    src/vrf_wrapper.cpp
)

add_library(common ${SOURCES} ${HEADERS})
target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(common PUBLIC
    devcore
    taraxa-vrf
    CONAN_PKG::openssl
    CONAN_PKG::jsoncpp
)

install(TARGETS common
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)