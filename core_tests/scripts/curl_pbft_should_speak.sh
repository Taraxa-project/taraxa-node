#!/bin/bash

set -B
  
cmd="{ \"jsonrpc\": \"2.0\",
	   \"id\": \"4\",
       \"method\": \"should_speak\",
       \"params\": [\"type\": \"1\",
       \"period\": 1234,
       \"step\": 56
    ]}}"

curl -d "$cmd" 0.0.0.0:7777 &>/dev/null
