#!/bin/bash

PREV_CWD=$(pwd)
cd "$(dirname "$0")"
cd ..

patterns="
  -iname '*.h' -o -iname '*.c'
  -o -iname '*.hpp' -o -iname '*.cpp'
  -o -iname '*.hxx' -o -iname '*.cxx'
  -o -iname '*.hh' -o -iname '*.cc'
"
eval find . -maxdepth 1 ${patterns}
eval find core_tests ${patterns}

cd ${PREV_CWD}
