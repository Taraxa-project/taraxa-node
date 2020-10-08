#!/bin/bash

(
  cd "$(dirname "$0")"
  cd ..

  grep path .gitmodules | sed 's/.*= //'
)
