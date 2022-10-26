# Change Log

This file documents all notable changes to `taraxa-node` Helm Chart. The release
numbering uses [semantic versioning](http://semver.org).

## v0.2.2

### Minor changes

* Uses new explorer chart (v0.2.1)
* Allows to specify PVC per node type
* Enables snapshots (on separate snapshot node)

## v0.2.1

### Patches

* Updates explorer dep version

## v0.2.0

### Major changes

* Rewrite config initial phase in Python

### Minor changes

* Adds GraphQL and GraphQL WS ports and ingresses

### Patches

* Fixes RPC nodes initconfig wrong validators

