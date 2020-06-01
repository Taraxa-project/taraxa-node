#!/bin/bash

cd "$(dirname "$0")"
cd ..
_home=$(pwd)

source scripts/lib/index.sh

if ! git status &>/dev/null; then
  exit 0
fi

_log=$(mktemp)
on_exit rm ${_log}
scripts/submodule_list.sh | while read line; do
  git submodule update --init ${line} | tee ${_log}
  [ -s ${_log} ] && (
    cd ${line}
    try_git_reset
  ) || true
done

# Fork/join update submodules' submodules (~2x speedup)
_child_exit_codes=$(mktemp)
on_exit rm ${_child_exit_codes}
(
  trap 'echo $? >> ${_child_exit_codes}' EXIT
  git submodule update --init --recursive submodules/libff
) &
(
  trap 'echo $? >> ${_child_exit_codes}' EXIT
  git submodule update --init --recursive submodules/openssl
) &
(
  trap 'echo $? >> ${_child_exit_codes}' EXIT
  git submodule update --init --recursive submodules/prometheus-cpp
) &
(
  trap 'echo $? >> ${_child_exit_codes}' EXIT
  cd submodules/taraxa-aleth
  git submodule update --init --recursive cmake/cable
) &
wait
cat ${_child_exit_codes} | while read line; do
  [ "${line}" == "0" ]
done
