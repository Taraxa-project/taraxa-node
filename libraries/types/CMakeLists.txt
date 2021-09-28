add_subdirectory(dag_block)
add_subdirectory(pbft_block)
add_subdirectory(proposal_period_levels_map)
add_subdirectory(transaction)
add_subdirectory(vote)

install(TARGETS dag_block pbft_block proposal_period_levels_map transaction vote
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)