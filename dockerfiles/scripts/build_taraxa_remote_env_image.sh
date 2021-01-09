#!/bin/bash

source ./taraxa.variables
source scripts/common.sh

# Load taraxa-builder docker image
LoadTaraxaBuilderImage

echo $CYAN"Building taraxa_remote_env image, "$taraxa_remote_env_image"..."$COLOR_END
DOCKER_BUILDKIT=1 docker build --progress=plain -t $taraxa_remote_env_image -f taraxa_remote_env.dockerfile .
echo $CYAN"Building taraxa_remote_env image, "$taraxa_remote_env_image" completed."$COLOR_END
