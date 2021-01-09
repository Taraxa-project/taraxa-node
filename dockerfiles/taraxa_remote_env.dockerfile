# Taraxa remote docker environment - it is intended to be used for taraxa-node building & remote development using docker image
#
# Build and run:
#
#   DOCKER_BUILDKIT=1 docker build --progress=plain -t taraxa/taraxa_remote_env:1.0 -f taraxa_remote_env.dockerfile .
#   docker run -d --cap-add sys_ptrace -p127.0.0.1:2222:22 --name taraxa_remote_env taraxa/taraxa_remote_env:1.0
#   ssh-keygen -f "$HOME/.ssh/known_hosts" -R "[localhost]:2222"
#
# stop:
#   docker stop taraxa_remote_env
#
# users:
#   root, passwd: root
#
# ssh login:
#   ssh -p 2222 root@localhost

FROM taraxa/taraxa-builder:1.0

# Install standard packages
RUN apt-get update

RUN apt-get install -y \
      sudo \
      ssh \
      rsync
RUN apt-get clean

# Setup ssh
RUN ( \
    echo 'LogLevel DEBUG2'; \
    echo 'PermitUserEnvironment yes'; \
    echo 'PermitRootLogin yes'; \
    echo 'PasswordAuthentication yes'; \
    echo 'Subsystem sftp /usr/lib/openssh/sftp-server'; \
    ) > /etc/ssh/sshd_config_taraxa
RUN mkdir -p /run/sshd

# Make env. variables visible when connecting through ssh
RUN mkdir -p $HOME/.ssh/
RUN env | grep 'GOROOT\|GOPATH\|PATH' >> $HOME/.ssh/environment

CMD ["/usr/sbin/sshd", "-D", "-e", "-f", "/etc/ssh/sshd_config_taraxa"]
