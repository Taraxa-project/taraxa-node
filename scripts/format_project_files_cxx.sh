#!/bin/bash

PREV_CWD=$(pwd)
cd "$(dirname "$0")"
cd ..

find . concur_storage core_tests crypto_tests \
  -maxdepth 1 \
  -iname '*.h' -o -iname '*.c' -o -iname '*.cpp' -o -iname '*.hpp' |
  xargs clang-format -style=file -i -fallback-style=none

cd ${PREV_CWD}
