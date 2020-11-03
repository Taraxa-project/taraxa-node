#!/bin/bash

(
  cd "$(dirname "$0")/.."
  # Forwards args to `docker` command
  DOCKER_BUILDKIT=1 docker build --progress=plain -t taraxa_node_main --build-arg BASE_IMAGE=taraxa_node_base ${@:1} -f dockerfiles/main.ubuntu.dockerfile .
)
