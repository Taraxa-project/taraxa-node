#!/bin/bash

source $(dirname "$0")/common.sh

echo "installing bzip2"
brew install bzip2
echo "Running command as sudo..."
sudo cp -r $(brew --prefix bzip2)/* ${dst}/
