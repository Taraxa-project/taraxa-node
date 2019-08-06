#!/bin/bash

cd "$(dirname "$0")"

rm -rf venv &&
virtualenv --no-site-packages --python=python3 venv &&
source venv/bin/activate &&
pip install -v -r requirements.txt &&
python -m demo