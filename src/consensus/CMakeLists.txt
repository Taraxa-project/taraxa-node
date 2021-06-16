set(HEADERS
    include/consensus/pbft_chain.hpp
    include/consensus/block_proposer.hpp
    include/consensus/vrf_wrapper.hpp
    include/consensus/vote.hpp
    include/consensus/pbft_manager.hpp
)

set(SOURCES
    src/pbft_manager.cpp
    src/vrf_wrapper.cpp
    src/vote.cpp
    src/pbft_chain.cpp
    src/block_proposer.cpp
)

add_library(consensus ${SOURCES} ${HEADERS})
target_include_directories(consensus PUBLIC "include")
target_link_libraries(consensus PUBLIC logger storage app_base)
