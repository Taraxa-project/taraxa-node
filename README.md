# Introducing Taraxa

Taraxa is a Practical Byzantine Fault Tolerance blockchain.


# Whitepaper
You can read the Taraxa Whitepaper at https://www.taraxa.io/whitepaper.


# Quickstart
Just want to get up and running quickly? We have pre-built docker images for your convenience.
More details are in our [quickstart guide](doc/quickstart_guide.md).


# Downloading
There are 2 options how to run the latest version of taraxa-node:

### Docker image
Download and run taraxa docker image with pre-installed taraxad binary [here](https://hub.docker.com/r/taraxa/taraxa-node).

### Ubuntu binary
Download and run statically linked taraxad binary [here](https://github.com/Taraxa-project/taraxa-node/releases).


# Building
If you would like to build from source, we do have [build instructions](doc/building.md) for Linux (Ubuntu LTS) and macOS.


# Running

### Inside docker image
    taraxad --conf_taraxa /etc/taraxad/taraxad.conf

### Pre-built binary or manual build:
    ./taraxad --conf_taraxa /path/to/config/file


# Contributing
Want to contribute to Taraxa repository ? We in Taraxa highly appreciate community work so if you feel like you want to
participate you are more than welcome. You can start by reading [contributing tutorial](doc/contributing.md).


# Useful doc
- [Git practices](doc/git_practices.md)
- [Coding practices](doc/coding_practices.md)
- [EVM incompatibilities](doc/evm_incompatibilities.md)
