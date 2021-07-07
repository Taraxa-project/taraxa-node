import traceback
from functools import wraps
from warnings import warn


def treat_as_warning(exception_type):
    def wrap(fn):
        @wraps(fn)
        def wrapped(*args, **kwargs):
            try:
                return fn(*args, **kwargs)
            except exception_type as e:
                # the standard warning display is too polluted, it may be hard to distinguish
                # the original message there
                warn(f"\n========= WARNING MESSAGE START =========\n"
                     f"An instance of expected exception occurred: {e}\n"
                     f"Stack trace: {''.join(traceback.format_exc(limit=-1))}\n"
                     f"TEST FAILED!!!\n"
                     f"========= WARNING MESSAGE END =========\n")

        return wrapped

    return wrap
