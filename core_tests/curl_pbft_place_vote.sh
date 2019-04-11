#!/bin/bash

set -B

for i in  {1..3}; do
  
  printf -v j "%064g" $i
  cmd="{ \"action\": \"place_vote\",
		 \"blockhash\": $i,
		 \"type\": $i,
		 \"period\": $i,
		 \"step\": $i
       }"

  curl -d "$cmd" 0.0.0.0:7777

done