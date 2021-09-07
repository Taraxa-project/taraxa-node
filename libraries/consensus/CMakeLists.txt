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
