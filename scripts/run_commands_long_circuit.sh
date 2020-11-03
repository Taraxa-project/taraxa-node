#!/bin/bash

# Interprets arguments as bash commands, runs them all regardless of failures,
# in the end fails if any of the commands failed.

exit_with=0
for arg in "$@"; do
  if ! $arg; then
    exit_with=1
  fi
done
exit $exit_with
