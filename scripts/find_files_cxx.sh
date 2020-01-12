#!/bin/bash

cd "$(dirname "$0")"
cd ..

find $(git ls-files) \
  -type f \
  -and \( \
  -path "*.cpp" \
  -or -path "*.cxx" \
  -or -path "*.hpp" \
  -or -path "*.hxx" \
  -or -path "*.h" \
  \) \
  -and ! \( \
  -path "submodules/*" \
  -or -path "unused_yet_useful/*" \
  \)
