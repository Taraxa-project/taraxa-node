add_subdirectory(dag)
add_subdirectory(dag_block)
add_subdirectory(pbft)
add_subdirectory(vote)

add_library(consensus INTERFACE)

target_link_libraries(consensus INTERFACE
    vote
    dag
    dag_block
    pbft
)

install(TARGETS dag_block vote
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
