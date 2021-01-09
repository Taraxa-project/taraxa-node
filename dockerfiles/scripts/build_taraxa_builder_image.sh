#!/bin/bash

source ./taraxa.variables
source scripts/common.sh

echo $CYAN"Building taraxa_builder image, "$taraxa_builder_image"..."$COLOR_END
docker DOCKER_BUILDKIT=1 docker build --progress=plain -t $taraxa_builder_image -f taraxa_builder.dockerfile .
echo $CYAN"Building taraxa_builder image, "$taraxa_builder_image" completed."$COLOR_END
