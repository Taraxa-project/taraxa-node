# taraxa-bootnode, a Taraxa discovery bootnode implementation
> A small, low memory usage C++ application which runs the Ethereum devp2p discovery protocol.

taraxa-bootnode is an application which runs the Ethereum devp2p discovery protocol (v4), which is the protocol that all Taraxa nodes use to find each other so they can communicate and do things like exchange blocks and transactions. You can read more about the discovery protocol here: https://github.com/ethereum/devp2p/wiki/Discovery-Overview

taraxa-bootnode doesn't support syncing (it doesn't understand the devp2p/RLPx TCP-based transport protocols such as the [Ethereum wire protocol](https://github.com/ethereum/wiki/wiki/Ethereum-Wire-Protocol)) and doesn't interact with a blockchain so it's small  (the binary size is < 2 MB) and has very low resource usage (typically < 5 MB of private working set memory).

## Install
taraxa-bootnode is a part of the [taraxa node](https://github.com/Taraxa-project/taraxa-node) and binaries are included in taraxa releases, so please see the [taraxa building](https://github.com/Taraxa-project/taraxa-node/blob/develop/doc/building.md) for instructions on building from source, downloading release binaries, and more.

## Usage
The following is the output of `taraxa-bootnode -h`:
```
NAME:
   taraxa-bootnode
USAGE:
   taraxa-bootnode [options]

GENERAL OPTIONS:
  -h [ --help ]         Show this help message and exit

NETWORKING:
  --public-ip <ip>          Force advertised public IP to the given IP (default: auto)
  --listen-ip <ip>(:<port>) Listen on the given IP for incoming connections (default: 0.0.0.0)
  --listen <port>           Listen on the given port for incoming connections (default: 10002)
  --allow-local-discovery   Include local addresses in the discovery process. Used for testing purposes.
  --network-id <id>         Connect to default mainet/testnet/devnet bootnodes
LOGGING OPTIONS:
  -v [ --log-verbosity ] <0 - 4> Set the log verbosity from 0 to 4 (default: 2).
  ```
