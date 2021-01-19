# Taraxa node docker environment - it contains taraxad binary + config file
#
# Build and run:
#
#   DOCKER_BUILDKIT=1 docker build --progress=plain -t taraxa/taraxa_node_dev:1.0 -f taraxa_node.dockerfile .
#   docker run -d --name taraxa_node_dev taraxa/taraxa_node_dev:1.0
#
# stop:
#   docker stop taraxa_node
#
# users:
#   root

FROM ubuntu:20.04

# Do not set this variable directly here, specify it's value in taraxa_variables.conf file
ARG TARAXA_BUILD_DIR

# Copy taraxad binary
RUN mkdir -p /etc/taraxad
COPY $TARAXA_BUILD_DIR/install/taraxad /usr/local/bin
COPY $TARAXA_BUILD_DIR/install/taraxad.conf /etc/taraxad

# Run taraxad
ENTRYPOINT ["taraxad"]
CMD ["--conf_taraxa", "/etc/taraxad/taraxad.conf"]
