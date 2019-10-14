#!/bin/bash

exit_code_sum=0
for arg in "$@"; do
  $arg
  let exit_code_sum+=$?
done
test $exit_code_sum -eq 0
