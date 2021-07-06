import flaky


def may_timeout(retries=1):
    assert 0 < retries
    return flaky.flaky(max_runs=retries + 1, min_passes=1,
                       rerun_filter=lambda err, *args, **kwargs: isinstance(err, TimeoutError))
