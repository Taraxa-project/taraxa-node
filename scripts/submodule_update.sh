#!/bin/bash

(
  cd "$(dirname "$0")"
  cd ..
  source scripts/lib/index.sh

  if ! is_git_repo; then
    exit 0
  fi

  cpu_count=$(scripts/cpu_count.sh)
  function upd() {
    git submodule update --jobs ${cpu_count} --init "$@"
  }

  upd
  upd --recursive submodules/taraxa-aleth
  upd --recursive submodules/libff
  upd --recursive submodules/boost
)
