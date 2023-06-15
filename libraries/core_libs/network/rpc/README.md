# DISABLED !!!! How to generate new API interface
```
sudo apt install libjsonrpccpp-tools
make gen_rpc_stubs
make clang-format
```

# PLEASE READ
As libjsonrpccpp doesn't support function arguments overload I had to made HACK and introduce `JSON_ANY`. `jsonrpc::Procedure` is created to check validation of passed arguments, but as it is not class enum we can used anything above 7 or number 0 to disable this check more https://github.com/cinemast/libjson-rpc-cpp/blob/d5ede2277d849f1a9d2dc111c4ec3ea652bd31ec/src/jsonrpccpp/common/specification.h#L29 . That's why generation via `gen_rpc_stubs` is disabled, because it would overwrite my hack in `EthFace.h`