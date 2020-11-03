#!/bin/bash

# TODO remove this horrible legacy
# There's a dependency somewhere outside this repo on exactly this
# output structure
echo VRF
echo VRF foo bar baz "$(./main vrf_keygen)"
