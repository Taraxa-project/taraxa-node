# Building taraxa-node docker image

To build taraxa-node docker image that can be used for running taraxad binary, go to dockerfiles folder:

    cd dockerfiles

, and simply call build_taraxa_node_image.sh script:
     
    ./scripts/build_taraxa_node_image.sh

It will build an image, which can be used for running docker container:

    docker run -it taraxa/taraxa_node_dev:1.0 /bin/bash

with running taraxad binary inside. Call this command inside running container:

    taraxad --conf_taraxa /etc/taraxad/taraxad.conf


# Building taraxa-remote-env docker image

To build taraxa-remote-env docker image, which can be used fo remote building integrated into your IDE, call script:

    ./scripts/build_taraxa_remote_env_image.sh

See [this tutorial](https://www.jetbrains.com/help/clion/clion-toolchains-in-docker.html#build-and-run) 
how to setup CLion with such docker image for remote building. 

Run taraxa_remote_env image in the background:

    docker run -d --cap-add sys_ptrace -p127.0.0.1:2222:22 --name taraxa_remote_env taraxa/taraxa_remote_env:1.0

To log into it using ssh:
    
    ssh -p 2222 root@localhost

, password:
    
    root
