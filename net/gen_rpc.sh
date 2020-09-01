#!/bin/bash

cd "$(dirname "$0")"
build_dir="gen_rpc_tmp"
cmake . -B ${build_dir}
cd ${build_dir}
make rpc_stubs
cd ..
rm -rf ${build_dir}
