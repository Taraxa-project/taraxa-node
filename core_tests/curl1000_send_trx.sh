#!/bin/bash

set -B

# this doesn't work....

j="0000000000000000000000000000000000000000000000000000000000000000"
sk="3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd"

#for i in $(seq -f "%064g" 1 1000); do
for i in  {1..1001}; do
  
  printf -v j "%064g" $i
  printf -v rec "%040g" $i
  cmd="{ \"action\": \"send_coin_transaction\", 
				 \"nonce\": 0,  
				 \"value\": $i,  
				 \"gas\": \"$j\",  
				 \"gas_price\": \"$j\", 
				 \"receiver\": \"$rec\", 
				 \"secret\": \"$sk\"
	       }"
  curl -d "$cmd" 0.0.0.0:7777 &>/dev/null

done


