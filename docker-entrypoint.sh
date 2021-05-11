#!/bin/bash

export TARAXA_CONF_PATH=${TARAXA_CONF_PATH:=/opt/taraxa/config.json}
export TARAXA_PERSISTENT_PATH=${TARAXA_PERSISTENT_PATH:=/opt/taraxa/data}
export TARAXA_COPY_COREDUMPS=${TARAXA_COPY_COREDUMPS:=true}
export TARAXA_SLEEP_DIAGNOSE=${TARAXA_SLEEP_DIAGNOSE:=false}

case $1 in

  cli)
    exec cli/taraxa "${@:2}"
    ;;

  taraxad)
    echo "Starting taraxad..."
    taraxad "${@:2}"
    ;;

  join)
	cli/taraxa config \
        node \
        --network $2 \
        --file $TARAXA_CONF_PATH

    echo "Starting taraxad..."
    taraxad \
            --conf_taraxa $TARAXA_CONF_PATH
    ;;

  single)
	cli/taraxa config \
        node \
        --as-boot-node \
        --file $TARAXA_CONF_PATH

    echo "Starting taraxad..."
    taraxad \
            --conf_taraxa $TARAXA_CONF_PATH
    ;;
  exec)
    exec "${@:2}"
    ;;

  *)
    echo "You should choose between:"
    echo "cli, taraxad, single, join {NAMED_NETWOTK}"
    ;;

esac

# Hack to copy coredumps on  K8s (gke) current /proc/sys/kernel/core_pattern
if [ "$TARAXA_COPY_COREDUMPS" = true ] ; then
    echo "Copying dump (if any) to $TARAXA_PERSISTENT_PATH"
    find / -maxdepth 1 -type f -name '*core*' -exec cp -v "{}" $TARAXA_PERSISTENT_PATH  \;
fi

# Hack to sleep forever so devs can diagnose the pod on k8s
# We should not set Liveness/Readiness for this to work
if [ "$TARAXA_SLEEP_DIAGNOSE" = true ] ; then
    echo "Sleeping forever for diagnosis"
    while true
    do
        echo "Crashed. Waiting on diagnosis..."
        sleep 300
    done
fi
