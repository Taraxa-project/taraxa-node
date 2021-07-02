from typing import Iterable

YES = lambda *args, **kwargs: True
NO = lambda *args, **kwargs: False
IS_TRUE_ISH = bool
FN_ID = lambda x: x


class SetHasBeenMatched:

    def __init__(self, set_elements: Iterable, allow_duplicates=False, collect=False):
        if isinstance(set_elements, set):
            self._set = set_elements
        else:
            self._set = set()
            for el in set_elements:
                assert el not in self._set
                self._set.add(el)
        self._allow_duplicates = allow_duplicates
        self._seen_so_far = set()
        self._collected = [] if collect else None

    @property
    def satisfied(self):
        return len(self._seen_so_far) == len(self._set)

    @property
    def collected(self):
        assert self._collected is not None, "collecting was not enabled"
        return self._collected

    def __call__(self, values):
        for v in values:
            assert v in self._set
            assert self._allow_duplicates or (v not in self._seen_so_far)
            self._seen_so_far.add(v)
            if self._collected is not None:
                self._collected.append(v)
        return self.satisfied
