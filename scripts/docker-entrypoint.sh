#!/bin/bash

# The following is a pseudocode equivalent of this script:
#
# function entrypoint(workdir, default_target, ...args) {
#    if (size(args) > 0) {
#        target = args[0]
#        if target == 'shell' {
#            // Launch interactive shell inside `workdir`.
#            // The shell will be restricted so that
#            // it will only see the `workdir` and nothing else.
#            // This mode makes sense for little cmd utils that work fast,
#            // since the performance overhead of `docker run`
#            // is quite dramatic.
#            return ...
#        } else if contains(workdir, target) {
#            // Run `target` inside `workdir` with the `args`.
#            // `target` is an executable or a script.
#            return ...
#        }
#    }
#    // run `default_target` inside `workdir` with the `args`.
#    // `default_target` is an executable or a script.
#}

WORKDIR=$1
DEFAULT_TARGET=$2

if [ $# -lt 2 ]; then
  echo "Not enough arguments"
  exit 1
fi

if [ $# -gt 2 ]; then

  # Launch an interactive shell. Makes sense for little cmd utils that work fast,
  # Since the performance overhead of `docker run` is quite dramatic.
  # The interactive shell will be sandboxed in the WORKDIR
  if [ "$3" == "shell" ]; then
    cd "${WORKDIR}"
    PATH=${PATH}:"$(pwd)" exec "${SHELL}" --restricted ${@:4:$#+1}
  fi

  for file in find "${WORKDIR}/"* -maxdepth 1 -type l; do
    if [ "${file}" == "${WORKDIR}/$3" ]; then
      exec "./${file}" ${@:4:$#+1}
    fi
  done

fi

exec "./${WORKDIR}/${DEFAULT_TARGET}" ${@:3:$#+1}
