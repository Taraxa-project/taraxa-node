#!/bin/bash

(
  cd "$(dirname "$0")"
  cd ..
  source scripts/lib/index.sh

  if ! is_git_repo; then
    exit 0
  fi

  cpu_count=$(scripts/cpu_count.sh)

  git submodule update --init --jobs ${cpu_count}

  git submodule update --init --recursive submodules/taraxa-aleth
  boost_libs=$(
    for lib in $(cat boost_dependencies.txt); do
      echo "libs/${lib}"
    done
  )
  cd submodules/boost
  git submodule update --init --recursive --jobs ${cpu_count} \
    tools/build tools/boost_install ${boost_libs}
)
