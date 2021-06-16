set(HEADERS
    include/final_chain/data.hpp
    include/final_chain/final_chain.hpp
    include/final_chain/replay_protection_service.hpp
    include/final_chain/state_api.hpp
    include/final_chain/state_api_data.hpp
    include/final_chain/trie_common.hpp
)

set(SOURCES
    src/data.cpp
    src/final_chain.cpp
    src/replay_protection_service.cpp
    src/state_api.cpp
    src/state_api_data.cpp
    src/trie_common.cpp
)

add_library(final_chain ${SOURCES} ${HEADERS})
target_include_directories(final_chain PUBLIC "include")
target_link_libraries(final_chain PRIVATE transaction_manager common config logger storage util app_base)