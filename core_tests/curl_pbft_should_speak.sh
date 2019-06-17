#!/bin/bash

set -B
  
cmd="{ \"action\": \"should_speak\",
       \"type\": \"1\",
       \"period\": 1234,
       \"step\": 56
    }"

curl -d "$cmd" 0.0.0.0:7777 &>/dev/null
