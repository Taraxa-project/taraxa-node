#!/bin/bash

cd "$(dirname "$0")"
cd ..

scripts/find_files_cxx.sh |
  while read line; do
    (clang-format -style=file -i "${line}") &
  done
wait
