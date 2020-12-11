# test cli image
FROM ubuntu:20.04
ENV DEBIAN_FRONTEND noninteractive
ENV TERM xterm
ENV APP_PATH /opt/taraxa/taraxa-node
ENV LD_LIBRARY_PATH /usr/local/lib

RUN apt update -y
RUN apt install -y python3-pip python3-dev build-essential

RUN apt install -y linux-headers-generic

RUN pip3 install eth-keys
RUN LIBSODIUM_MAKE_ARGS=-j4 pip3 install pynacl
RUN pip3 install click 

RUN mkdir -p ${APP_PATH}/config
WORKDIR ${APP_PATH}
COPY cli/taraxa ./taraxa

ENTRYPOINT ["./taraxa"]
CMD ["--help"]