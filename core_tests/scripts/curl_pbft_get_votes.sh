#!/bin/bash

set -B
  
cmd="{ \"jsonrpc\": \"2.0\",
		 \"id\": \"2\",
         \"method\": \"get_votes\", 
         \"params\": [{\"period\": 2}]
    }"

curl -d "$cmd" 0.0.0.0:7777 &>/dev/null
