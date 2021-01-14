#!/bin/bash

source $(dirname "$0")/common.sh

echo "installing openssl 1.1"
brew install openssl@1.1
cp -r $(brew --prefix openssl@1.1)/* ${dst}/
