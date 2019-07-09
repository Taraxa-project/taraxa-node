#!/bin/bash

BRANCH_NAME=$1
[[ ${BRANCH_NAME} == "master" ]] &&
  echo latest ||
  echo ${BRANCH_NAME} | tr "[:upper:]/" "[:lower:]_"
