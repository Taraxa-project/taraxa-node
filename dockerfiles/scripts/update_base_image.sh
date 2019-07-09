#!/bin/bash

set -e

cd "$(dirname "$0")"

_CONFIG_FILE=env_variables.sh

if [ ! -f ${_CONFIG_FILE} ]; then
    echo -e "Config file not found ${_CONFIG_FILE}.\nCreating .."
    ./env-setup.sh
fi

echo "Sourcing ${_CONFIG_FILE}"
. ${_CONFIG_FILE}

DOCKER_HOST="541656622270.dkr.ecr.us-west-2.amazonaws.com"
DOCKER_IMAGE_TAG=$(./docker_tag_from_branch.sh "$(git branch --show-current)")
DOCKER_IMAGE="${DOCKER_HOST}/taraxa-node-base:${DOCKER_IMAGE_TAG}"

echo -e "*** Docker Base Image Update ***\n\n"

echo -e "\n* Checking AWS credentials ..."
: ${AWS_ACCESS_KEY_ID:?Need to set AWS_ACCESS_KEY_ID non-empty}
: ${AWS_SECRET_ACCESS_KEY:?Need to set AWS_SECRET_ACCESS_KEY non-empty}
: ${AWS_DEFAULT_REGION:?Need to set AWS_DEFAULT_REGION non-empty}

echo -e "\n* Docker Registry log in ..."
eval $(aws ecr get-login --region $AWS_DEFAULT_REGION --no-include-email)

docker pull $DOCKER_IMAGE:latest

echo -e "\n* Building Docker Image $DOCKER_IMAGE ..."
docker build -t $DOCKER_IMAGE --cache-from=$DOCKER_IMAGE:latest - <../base.ubuntu.dockerfile

echo -e "\n* Pushing Docker Image $DOCKER_IMAGE ..."
docker push $DOCKER_IMAGE

echo -e "\n* Removing Docker Image $DOCKER_IMAGE ..."
docker rmi $DOCKER_IMAGE

echo -e "\n* $DOCKER_IMAGE has been updated\n"
echo -e "\n\n*** Docker Base Image has been updated ***\n\n"
