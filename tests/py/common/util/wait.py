import time
from dataclasses import dataclass
from typing import Callable, Any

from common.util.predicates import IS_TRUE_ISH, NO


@dataclass
class Timeout:
    num_attempts: int = 30
    backoff_seconds: int = 3


def wait(get_value: Callable[[], Any],
         is_value_ok=IS_TRUE_ISH,
         is_exception_ok=NO,
         fail_immediately=NO,
         timeout=Timeout()):
    assert 0 <= timeout.num_attempts
    infinite_attempts = timeout.num_attempts == 0
    while True:
        value, is_exception = None, False
        try:
            value = get_value()
            if is_value_ok(value):
                return value
        except Exception as e:
            if isinstance(e, AssertionError):
                raise e
            value, is_exception = e, True
            if is_exception_ok(value):
                return value
        if fail_immediately(value):
            if is_exception:
                raise value
            raise ValueError(value)
        if not infinite_attempts:
            timeout.num_attempts -= 1
            if timeout.num_attempts == 0:
                raise TimeoutError()
        time.sleep(timeout.backoff_seconds)
