#!/bin/bash

AWS_PROFILE=${AWS_PROFILE:-default}
if [ -z "${AWS_ACCESS_KEY_ID}" ]; then
  AWS_ACCESS_KEY_ID=$(aws configure get ${AWS_PROFILE}.aws_access_key_id)
fi
if [ -z "${AWS_SECRET_ACCESS_KEY}" ]; then
  AWS_SECRET_ACCESS_KEY=$(aws configure get ${AWS_PROFILE}.aws_secret_access_key)
fi
if [ -z "${AWS_DEFAULT_REGION}" ]; then
  AWS_DEFAULT_REGION="us-west-2"
fi
eval $(aws ecr get-login --region $AWS_DEFAULT_REGION --no-include-email)
