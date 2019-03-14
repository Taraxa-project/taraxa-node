FROM 258198740961.dkr.ecr.us-west-2.amazonaws.com/taraxa-node-base as builder

ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib/
WORKDIR ${APP_PATH}
ADD . .  
RUN make main


FROM ubuntu:18.04

ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm
ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib/

RUN mkdir -p ${APP_PATH}/config
WORKDIR ${APP_PATH}
COPY --from=builder ${APP_PATH}/build/main .
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /usr/lib/libscrypt.* /usr/lib/
COPY --from=builder /usr/lib/x86_64-linux-gnu/* /usr/lib/x86_64-linux-gnu/ 
COPY ./core_tests/*.json ./default_config/


ENTRYPOINT [ "./main" ]
CMD ["--conf_full_node", "./default_config/conf_full_node1.json", "--conf_network", "./default_config/conf_network1.json", "--conf_rpc", "./default_config/conf_rpc1.json"]