add_subdirectory(final_chain)
add_subdirectory(dag)
add_subdirectory(pbft)
add_subdirectory(vote_manager)
add_subdirectory(transaction_manager)

add_library(consensus INTERFACE)

target_link_libraries(consensus INTERFACE
    dag
    final_chain
    pbft
    transaction_manager
    vote_manager
)
