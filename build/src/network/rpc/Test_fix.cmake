
file(READ /Users/jsnapp/Documents/Taraxa/taraxa-node/src/network/rpc/TestFace.h server_file_content)
string(REPLACE
       "include \"ModularServer.h\""
       "include <libweb3jsonrpc/ModularServer.h>"
       server_file_content "${server_file_content}")
file(WRITE /Users/jsnapp/Documents/Taraxa/taraxa-node/src/network/rpc/TestFace.h "${server_file_content}")
    