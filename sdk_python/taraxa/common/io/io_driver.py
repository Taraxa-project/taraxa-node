from abc import ABC, abstractmethod
from subprocess import Popen, PIPE


class IODriver(ABC):

    def __init__(self, **popen_args):
        self.popen_args = dict(encoding='utf-8', stdin=PIPE, stdout=PIPE,
                               # stderr=PIPE, TODO proper error handling
                               **popen_args)

    def spawn(self, *args):
        return Popen(args, **self.popen_args)

    @abstractmethod
    def call(self, *args) -> Popen:
        pass

    @abstractmethod
    def close(self):
        pass
