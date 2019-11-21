#!/bin/bash

cd "$(dirname "$0")"
cd ..
_home=$(pwd)

source scripts/lib/errors.sh

[ ! git status ] &>/dev/null && exit 0 || true

_log=$(mktemp)
finally rm ${_log}
scripts/submodule_list.sh | while read line; do
  [ "${line}" == "submodules/aleth" ] && continue || true
  git submodule update --init ${line} | tee ${_log}
  [ -s ${_log} ] && git clean -q -dfx ${line} || true
done

# Fork/join update submodules' submodules (~2x speedup)
_child_exit_codes=$(mktemp)
finally rm ${_child_exit_codes}
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
#(
#  trap 'echo $? >> ${_child_exit_codes}' EXIT
#  cd submodules/aleth
#  git submodule update --init --recursive evmc
#  git submodule update --init --recursive cmake/cable
#) &
wait
cat ${_child_exit_codes} | while read line; do
  [ "${line}" == "0" ]
done
