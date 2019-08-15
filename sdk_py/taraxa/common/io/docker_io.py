from subprocess import Popen

from taraxa.common.io.io_driver import IODriver


class DockerIO(IODriver):

    def __init__(self, image, **popen_args):
        super(DockerIO, self).__init__(**popen_args)
        self.image = image

    def call(self, *args) -> Popen:
        return self.spawn("docker", "run", self.image, *args)

    def close(self):
        pass
