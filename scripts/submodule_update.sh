#!/bin/bash

(
  cd "$(dirname "$0")"
  cd ..

  source scripts/lib/index.sh

  if ! is_git_repo; then
    exit 0
  fi

  for p in $(scripts/submodule_list.sh); do
    git submodule update --init "${p}"
  done

  git submodule update --recursive submodules/libff
  git submodule update --recursive submodules/openssl
  git submodule update --recursive submodules/taraxa-aleth
  (
    libs_=$(cat boost_dependencies.txt)
    cd submodules/boost
    git submodule update --init --recursive tools/build
    git submodule update --init --recursive tools/boost_install
    for lib in ${libs_}; do
      git submodule update --init --recursive "libs/${lib}"
    done
  )
)
