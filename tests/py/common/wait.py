import time
from typing import Callable, Any

GetValue = Callable[[], Any]

YES = lambda x: True
NO = lambda x: False
IS_TRUE_ISH = bool


def wait(get_value: GetValue,
         is_value_ok=IS_TRUE_ISH,
         is_exception_ok=NO,
         fail_immediately=NO,
         num_attempts=8,
         backoff_seconds=2):
    assert 0 <= num_attempts
    infinite_attempts = num_attempts == 0
    while True:
        value, is_exception = None, False
        try:
            value = get_value()
            if is_value_ok(value):
                return value
        except Exception as e:
            value, is_exception = e, True
            if is_exception_ok(value):
                return value
        if fail_immediately(value):
            if is_exception:
                raise value
            raise ValueError(value)
        if not infinite_attempts:
            num_attempts -= 1
            if num_attempts == 0:
                raise TimeoutError()
        time.sleep(backoff_seconds)
