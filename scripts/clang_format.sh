#!/bin/bash

while read -r line; do
  (clang-format -style=file -i "$line") &
done <&0
wait
