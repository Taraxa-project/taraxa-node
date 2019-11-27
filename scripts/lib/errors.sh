# set LOG_TRACE=1 to see a more detailed log

set -eo pipefail
set -eo errtrace

_success_handler_stack=$(mktemp)
_error_handler_stack=$(mktemp)
_exit_handler_stack=$(mktemp)

function on_success() {
  echo "$@" >>${_success_handler_stack}
}

function on_error() {
  echo "$@" >>${_error_handler_stack}
}

function on_exit() {
  echo "$@" >>${_exit_handler_stack}
}

function log_trace() {
  [ "${LOG_TRACE}" == "1" ] && echo $@ || true
}

function __execute_stack__() {
  local type=$1
  local stack_file=$(eval echo \${_${type}_handler_stack})
  cat ${stack_file} | while read line; do
    log_trace "executing ${type} callback: ${line}"
    eval "${line}" || true
  done
}
function __do_exit__() {
  local file="$0"
  local line="$1"
  local exit_code="$2"
  log_trace "EXIT at ${file}:${line} with code ${exit_code}"
  if [ ${exit_code} == "0" ]; then
    __execute_stack__ success
  else
    __execute_stack__ error
  fi
  __execute_stack__ exit
  rm ${_success_handler_stack}
  rm ${_error_handler_stack}
  rm ${_exit_handler_stack}
}

trap '__do_exit__ ${LINENO} $?' EXIT
