#!/bin/bash

"$(dirname "$0")"/find_files_cxx_node.sh |
  grep ".*_test.cpp" |
  grep -v "core_tests/performance_test.cpp"
