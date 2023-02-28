# Change Log

This file documents all notable changes to `taraxa-node` Helm Chart. The release
numbering uses [semantic versioning](http://semver.org).

## v0.2.6

### Minor changes

* Fix for `image.pullPolicy`

## v0.2.5

### Minor changes

* Allow for different images in `StatefulSet`s for boot, rpc and consensus nodes

## v0.2.4

### Minor changes

* Specify persistance (PVC) per node type

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

## v0.1.10

### Major changes

* Change in prefix for chain IDs



