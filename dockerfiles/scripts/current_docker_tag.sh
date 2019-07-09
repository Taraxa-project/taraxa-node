#!/bin/bash

BRANCH_NAME=$(git branch --show-current)
[[ ${BRANCH_NAME} == "master" ]] &&
  echo latest ||
  echo ${BRANCH_NAME} | tr "[:upper:]/" "[:lower:]_"
