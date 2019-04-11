#!/bin/bash

set -B
  
cmd="{ \"action\": \"get_votes\",
       \"period\": 2
    }"

curl -d "$cmd" 0.0.0.0:7777
