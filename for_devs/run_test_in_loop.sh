#!/bin/bash

# This simple script runs a specified test in an infinite loop until it fails. It prints run number before each new run
# + when it fails, script ends and it prints the error + number of runs until it failed.
# It is used to find bugs like rare race conditions that happen only occasionally, etc...

# Usage: ./for_devs/run_test_in_loop.sh './cmake-build-debug/bin/vote_test --gtest_filter=* --gtest_color=no'

TEST_CMD="$@"
printf "\n********************************** Running \"$TEST_CMD\" in infinite loop *********************************"

COUNTER=1

while true; do
    printf "\n\n******************************************** TEST RUN %d  ************************************\n\n" $COUNTER

#    ./cmake-build-debug/bin/full_node_test --gtest_filter=FullNodeTest.sync_five_nodes:FullNodeTest/*.sync_five_nodes:FullNodeTest.sync_five_nodes/*:*/FullNodeTest.sync_five_nodes/*:*/FullNodeTest/*.sync_five_nodes --gtest_color=no
# ec=$?  # grab the exit code into a variable so that it can
#        # be reused later, without the fear of being overwritten

    $TEST_CMD
    ec=$? # grab the exit code into a variable so that it can be reused later, without the fear of being overwritten
    case $ec in
        0) printf '%s\n' "Command exited with zero";;
        1) printf '%s RUNS: %d\n' "Command exited with non-zero" $COUNTER; exit 1;;
        *) printf '%s RUNS: %d\n' "Command exited with *" $COUNTER; exit 1;;
    esac

    let COUNTER++
done
