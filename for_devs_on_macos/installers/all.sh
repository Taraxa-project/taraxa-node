#!/bin/bash

cd $(dirname "$0")

# running sequentially because it requests sudo rights
./openssl.sh
find . -type f ! -path "./common.sh" -and ! -path "./all.sh" -and ! -path "./openssl.sh" |
  parallel --will-cite -j999
