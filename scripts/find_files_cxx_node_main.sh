#!/bin/bash

"$(dirname "$0")"/find_files_cxx_node.sh | grep -v ".*_test.cpp"
