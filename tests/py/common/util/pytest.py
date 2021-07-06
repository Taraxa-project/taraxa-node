import traceback
from functools import wraps
from warnings import warn


def may_timeout(**kwargs):
    return expect_exception(TimeoutError, **kwargs)


def expect_exception(exception_type, treat_as_warning=False, retries=0):
    assert 0 <= retries
    assert 0 < retries or treat_as_warning, "the given parameters make this decorator give no effect"

    def wrap(fn):
        @wraps(fn)
        def wrapped(*args, **kwargs):
            for attempt_i in range(retries + 1):
                last_attempt = attempt_i == retries
                if last_attempt and not treat_as_warning:
                    return fn(*args, **kwargs)
                try:
                    return fn(*args, **kwargs)
                except exception_type as e:
                    # python's builtin warning display is too polluted, it may be hard to distinguish
                    # the original message there
                    msg = (f"\n========= WARNING MESSAGE START =========\n"
                           f"An instance of expected exception occurred: {e}\n"
                           f"Stack trace: {''.join(traceback.format_exc(limit=-1))}")
                    if not last_attempt:
                        msg += "\nRERUNNING...\n"
                    else:
                        msg += "\nTEST FAILED!!!\n"
                    msg += "========= WARNING MESSAGE END =========\n"
                    warn(msg)

        return wrapped

    return wrap
