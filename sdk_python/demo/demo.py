from taraxa.common.io.docker_interactive_io import DockerInteractiveIO
from taraxa.keygen import Keygen

io = DockerInteractiveIO("541656622270.dkr.ecr.us-west-2.amazonaws.com/taraxa-node:latest")
keygen = Keygen(io)
for _ in range(100):
    print(keygen.exec())
