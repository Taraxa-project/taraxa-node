#!/bin/bash

cd "$(dirname "$0")"
cd ..

find . \
  -type f \
  -and \( \
  -path "*.cpp" \
  -or -path "*.cxx" \
  -or -path "*.hpp" \
  -or -path "*.hxx" \
  -or -path "*.h" \
  \) \
  -and ! \( \
  -path "./submodules/*" \
  -or -path "./unused_yet_useful/*" \
  -or -path "./build/*" \
  -or -path "./cmake-*" \
  \) |
  cut -c3-
