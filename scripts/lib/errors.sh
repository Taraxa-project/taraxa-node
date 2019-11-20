#!/bin/bash

# set LOG_TRACE=1 to see a more detailed log

set -eo pipefail
set -eo errtrace

_exit_stack=$(mktemp)

function log_trace() {
  [ "${LOG_TRACE}" == "1" ] && echo $@ || true
}

function exit_handler() {
  log_trace "EXIT at $0:$1"
  cat ${_exit_stack} | while read line; do
    log_trace "executing exit callback: ${line}"
    eval "${line}" || true
  done
  rm ${_exit_stack}
}

function finally() {
  echo "$@" >>${_exit_stack}
}

trap 'exit_handler ${LINENO}' EXIT
