#!/bin/bash

DIR=$1
DEFAULT_TARGET=$2

if [ "$3" == "shell" ]; then
  cd "${DIR}" && exec "${SHELL}" ${@:4:$#+1}
fi

for file in find "${DIR}/"* -maxdepth 1 -type l; do
  if [ "${file}" == "${DIR}/$3" ]; then
    exec "./${file}" ${@:4:$#+1}
  fi
done

exec "./${DIR}/${DEFAULT_TARGET}" ${@:3:$#+1}
