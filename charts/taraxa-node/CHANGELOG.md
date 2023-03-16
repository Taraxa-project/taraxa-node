# Change Log

This file documents all notable changes to `taraxa-node` Helm Chart. The release
numbering uses [semantic versioning](http://semver.org).

## v0.3.7

### Minor changes

* Adds transaction generating service to replace the explorer faucet

## v0.3.6

### Minor changes

* Added labels into `StatefulSets` for [kube-monkey](https://github.com/asobti/kube-monkey)

## v0.3.5

### Minor changes

* Changed `db_path` to `data_dir` for taraxa-indexer

## v0.3.4

### Minor changes

* Enabled CORS on `Ingress` of indexer

## v0.3.3

### Major changes

* Added sidecar container to RPC nodes with [taraxa-indexer](https://github.com/Taraxa-project/taraxa-indexer)

## v0.3.2

### Minor changes

* Added `ServiceMonitor` to enable scraping targets by _PrometheusOperator_

## v0.3.1

### Minor changes

* Added new `Service` of type `NodePort` to for rpc nodes

## v0.3.0

### Major changes

* Separate config for genesis

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



