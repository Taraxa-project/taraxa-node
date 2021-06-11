#!/bin/bash

# The arguments to this script will be passed directly to `pytest`. Run this script with `--setup-only` to only
# set up the environment without running tests. For interactive development, google how to activate/use virtual env,
# which will be generated by this script in `./venv`.

(
  cd "$(dirname "$0")"
  pip3 install virtualenv
  virtualenv -p python3 venv
  source venv/bin/activate
  pip install -r requirements.txt
  python -m pytest $@
)
