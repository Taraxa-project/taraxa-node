#!/bin/bash

export KIBANA_URL=${KIBANA_URL:=kibana.gcp.taraxa.io}

if [ -z "$HELM_TEST_NAME" ]; then
        echo "No HELM_TEST_NAME var"
        exit 1
fi

TIME_FROM=$(date -d"-1 hour" +%Y-%m-%dT%T.000Z)
TIME_TO=$(date -d"+10 minutes" +%Y-%m-%dT%T.000Z)

echo "https://${KIBANA_URL}/app/kibana#/discover?_g=(filters:!(),refreshInterval:(pause:!t,value:0),time:(from:'${TIME_FROM}',to:'${TIME_TO}'))&_a=(columns:!(kubernetes.pod_name,log),filters:!(('\$state':(store:appState),meta:(alias:!n,disabled:!f,key:kubernetes.namespace_name,negate:!f,params:(query:${HELM_TEST_NAME}),type:phrase,value:${HELM_TEST_NAME}),query:(match:(kubernetes.namespace_name:(query:${HELM_TEST_NAME},type:phrase))))),interval:auto,query:(language:kuery,query:''),sort:!('@timestamp',desc))"
