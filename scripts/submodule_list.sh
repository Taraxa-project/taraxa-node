#!/bin/bash

cd "$(dirname "$0")"
cd ..

git config --file .gitmodules --get-regexp path | awk '{ print $2 }'
