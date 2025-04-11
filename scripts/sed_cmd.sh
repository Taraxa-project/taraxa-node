#!/bin/bash

IS_MACOS=$(uname | grep -q Darwin && echo true || echo false)

if [ "$IS_MACOS" = true ]; then
    # macOS
    export SED_CMD="-i ''"
else
    # Linux/Ubuntu  
    export SED_CMD="-i"
fi

