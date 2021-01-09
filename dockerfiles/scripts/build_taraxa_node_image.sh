#!/bin/bash

source ./taraxa.variables
source scripts/common.sh

# Build taraxa-node from local sources using $taraxa_builder_image docker image
./scripts/build_local_taraxa_node.sh
if [ ! $? -eq 0 ]
then
  exit 1
fi

echo $CYAN"Building taraxa_node image, "$taraxa_node_image"..."$COLOR_END
DOCKER_BUILDKIT=1 docker build --progress=plain -t $taraxa_node_image -f taraxa_node.dockerfile .
echo $CYAN"Building taraxa_node image, "$taraxa_node_image" completed."$COLOR_END
