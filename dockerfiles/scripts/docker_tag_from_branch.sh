#!/bin/bash

BRANCH_NAME=$1
[[ ${BRANCH_NAME} == "master" ]] &&
  echo latest ||
  echo ${BRANCH_NAME} | tr "[:upper:]" "[:lower:]" | sed 's/[^a-zA-Z0-9_-]/-/g'
