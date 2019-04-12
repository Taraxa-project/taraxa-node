#!/bin/bash

set -B

for i in  {1..3}; do

  cmd="{ \"action\": \"place_vote\",
         \"blockhash\": $i,
         \"type\": $i,
         \"period\": $i,
         \"step\": $i
       }"

  curl -d "$cmd" 0.0.0.0:7777

done
