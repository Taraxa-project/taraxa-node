#!/bin/bash

EXECUTABLE="main"
if [ ${DEBUG} -eq 1 ]; then
  # Debug mode
  EXECUTABLE="main-d"
fi
exec ./${EXECUTABLE} ${@:1:$#}
