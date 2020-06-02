#!/bin/bash

"$(dirname "$0")"/find_files_cxx.sh |
  grep ".*.cpp" |
  grep -v "prometheus_demo.cpp" |
  grep -v "main.cpp"
