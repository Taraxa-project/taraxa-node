#!/bin/bash

PREV_CWD=$(pwd)
cd "$(dirname "$0")"
cd ..

find . concur_storage core_tests \
  -maxdepth 1 \
  -iname '*.h' -o -iname '*.c' -o -iname '*.cpp' -o -iname '*.hpp' \
  -o -iname '*.hxx' -o -iname '*.cxx' |
  xargs clang-format -style=file -i -fallback-style=none

cd ${PREV_CWD}
