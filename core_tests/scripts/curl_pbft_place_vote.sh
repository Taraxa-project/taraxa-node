#!/bin/bash

set -B

for i in  {1..3}; do

  cmd="{ \"jsonrpc\": \"2.0\",
		     \"id\": \"3\",
         \"method\": \"place_vote\",
         \"params\": [{\"blockhash\": $i,
         \"type\": $i,
         \"period\": $i,
         \"step\": $i
       }]}"

  curl -d "$cmd" 0.0.0.0:7777 &>/dev/null

done
