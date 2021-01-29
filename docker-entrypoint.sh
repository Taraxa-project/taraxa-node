#!/bin/bash

export TARAXA_CONF_PATH=${TARAXA_CONF_PATH:=/opt/taraxa/config.json}

case $1 in

  cli)
    exec cli/taraxa "${@:2}"
    ;;

  taraxad)
    echo "Starting taraxad..."
    exec taraxad "${@:2}"
    ;;

  join)
	cli/taraxa config \
        node \
        --network $2 \
        --file $TARAXA_CONF_PATH

    echo "Starting taraxad..."
    exec taraxad \
            --conf_taraxa $TARAXA_CONF_PATH
    ;;

  single)
	cli/taraxa config \
        node \
        --as-boot-node \
        --file $TARAXA_CONF_PATH

    echo "Starting taraxad..."
    exec taraxad \
            --conf_taraxa $TARAXA_CONF_PATH
    ;;
  exec)
    exec "${@:2}"
    ;;

  *)
    echo "You should choose between:"
    echo "cli, taraxad, sigle, join {NAMED_NETWOTK}"
    ;;

esac
