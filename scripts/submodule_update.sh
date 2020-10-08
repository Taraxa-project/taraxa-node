#!/bin/bash

(
  cd "$(dirname "$0")"
  cd ..

  source scripts/lib/index.sh

  if ! is_git_repo; then
    exit 0
  fi

  cpu_count=$(scripts/cpu_count.sh)

  git submodule update --init --jobs "${cpu_count}"

  function submodule_upd() {
    git submodule update --init --recursive "$1" &
  }

  submodule_upd submodules/taraxa-aleth
  #  git submodule update --init --recursive --jobs 12 submodules/boost
  boost_libs=$(cat boost_dependencies.txt)
  cd submodules/boost
  submodule_upd tools/build
  submodule_upd tools/boost_install
  for lib in ${boost_libs}; do
    submodule_upd "libs/${lib}"
  done
  wait
)
