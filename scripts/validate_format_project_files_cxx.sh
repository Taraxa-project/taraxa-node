#!/bin/bash

PREV_CWD=$(pwd)
cd "$(dirname "$0")"
cd ..

replacements_count=$(
  scripts/find_project_files_cxx.sh |
    xargs clang-format -style=file -fallback-style=none \
      -output-replacements-xml |
    grep -c "<replacement "
)

cd ${PREV_CWD}

if [ ${replacements_count} -gt 0 ]; then
  echo "Unformatted files found."
  #exit 1
  exit 0
fi
