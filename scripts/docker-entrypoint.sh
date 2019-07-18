#!/bin/bash

WORKDIR=$1
DEFAULT_TARGET=$2

if [ $# -lt 2 ]; then
  echo "Not enough arguments"
  exit 1
fi

# Launch an interactive shell. Makes sense for little cmd utils that work fast,
# Since the performance overhead of `docker run` is quite dramatic.
# The interactive shell will be sandboxed in the current directory
if [ "$3" == "shell" ]; then
  cd "${WORKDIR}"
  PATH=${PATH}:"$(pwd)" exec "${SHELL}" --restricted ${@:4:$#+1}
fi

for file in find "${WORKDIR}/"* -maxdepth 1 -type l; do
  if [ "${file}" == "${WORKDIR}/$3" ]; then
    exec "./${file}" ${@:4:$#+1}
  fi
done

exec "./${WORKDIR}/${DEFAULT_TARGET}" ${@:3:$#+1}
