#!/bin/bash

exit_with=0
for arg in "$@"; do
  if ! $arg; then
    exit_with=1
  fi
done
exit $exit_with
