from subprocess import Popen

from taraxa.common.io.io_driver import IODriver


class DockerInteractiveIO(IODriver):

    def __init__(self, image, **popen_args):
        super(DockerInteractiveIO, self).__init__(**popen_args)
        self.process = self.spawn("docker", "run", "-i", image, "shell")

    def call(self, *args) -> Popen:
        self.process.stdin.write(" ".join(args) + "\n")
        self.process.stdin.flush()
        return self.process

    def close(self):
        self.process.kill()
        self.process.wait()
