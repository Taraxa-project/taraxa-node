#!/bin/bash

set -B

# this doesn't work....

j="0000000000000000000000000000000000000000000000000000000000000000"

for i in $(seq -f "%064g" 1 100); do
  
  cmd="{ \"jsonrpc\": \"2.0\",
				 \"method\": \"insert_dag_block\", 
         \"id\": \"1\", 
				 \"params\": [{
         \"pivot\": \"$j\", 
         \"hash\": \"$i\", 
         \"sender\":\"0000000000000000000000000000000000000001\", 
         \"tips\": []}]}"
  printf -v j "%064g" $i
  
  curl -d "$cmd" 0.0.0.0:7777 &>/dev/null
  echo $i

done


