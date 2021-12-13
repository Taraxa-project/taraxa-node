#!/bin/bash

export TARAXA_CONF_PATH=${TARAXA_CONF_PATH:=/root/.taraxa/conf_taraxa.json}
export TARAXA_PERSISTENT_PATH=${TARAXA_PERSISTENT_PATH:=/root/.taraxa}
export TARAXA_COPY_COREDUMPS=${TARAXA_COPY_COREDUMPS:=true}
export TARAXA_SLEEP_DIAGNOSE=${TARAXA_SLEEP_DIAGNOSE:=false}

case $1 in

  taraxa-bootnode)
    echo "Starting taraxa-bootnode..."

    FLAGS=""
    if [[ -z "${HOSTNAME}" ]]; then
      echo "HOSTNAME is not set."
    else
      INDEX=${HOSTNAME##*-}
      ADVERTISED_IP_NAME="ADVERTISED_IP_$INDEX"
      ADVERTISED_IP=${!ADVERTISED_IP_NAME}

      if [[ -z "${ADVERTISED_IP}" ]]; then
        echo "ADVERTISED_IP is not set."
      else
        FLAGS="--public-ip ${ADVERTISED_IP}"
      fi
    fi

    taraxa-bootnode $FLAGS "${@:2}"
    ;;

  taraxad)
    echo "Starting taraxad..."
    taraxad "${@:2}"
    ;;

  join)
    echo "Starting taraxad..."
    taraxad \
            --config $TARAXA_CONF_PATH
            --network-id $2

    ;;

  single)
	  echo "Starting taraxad..."
    taraxad \
            --config $TARAXA_CONF_PATH
            --boot-node true

    ;;
  exec)
    exec "${@:2}"
    ;;

  *)
    echo "You should choose between:"
    echo "taraxa-bootnode, taraxad, single, join {NAMED_NETWOTK}"
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
