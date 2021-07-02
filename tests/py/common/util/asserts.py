from typing import Iterable


def assert_equal(first, *rest):
    if not rest:
        assert isinstance(first, Iterable), "if single argument is given, it must be iterable"
        rest = iter(first)
        first = next(rest)
    had_more_than_one = False
    for val in rest:
        assert val == first
        had_more_than_one = True
    assert had_more_than_one, "need at least two objects to compare"
    return first
