#!/bin/bash

# Handy way to build the base image
(
  cd "$(dirname "$0")/.."
  # Base image build requires clean submodule directories
  # which means local submodule build cache must be cleaned
  git submodule update --init --recursive
  git submodule foreach --recursive --quiet git clean -dfx
  # Forwards args to `docker` command
  DOCKER_BUILDKIT=1 docker build --progress=plain -t taraxa_node_base ${@:1} -f dockerfiles/base.ubuntu.dockerfile .
)
