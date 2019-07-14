#!/bin/bash

PREV_CWD=$(pwd)
cd "$(dirname "$0")"
cd ..

scripts/find_project_files_cxx.sh |
  xargs clang-format -style=file -i -fallback-style=none

cd ${PREV_CWD}
