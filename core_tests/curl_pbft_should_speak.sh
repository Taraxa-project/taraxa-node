#!/bin/bash

set -B
  
cmd="{ \"action\": \"should_speak\",
       \"blockhash\": \"1234567890abcdef\",
       \"type\": \"a\",
       \"period\": 1234,
       \"step\": 56
    }"

curl -d "$cmd" 0.0.0.0:7777 &>/dev/null
