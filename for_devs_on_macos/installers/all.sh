#!/bin/bash

cd $(dirname "$0")
find . -type f ! -path "./common.sh" -and ! -path "./all.sh" | parallel --will-cite -j999
