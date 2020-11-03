#!/bin/bash

# reads files from stdin, spawns a parallel subprocess for each file

while read -r line; do
  (clang-format -style=file -i "$line") &
done <&0
wait
