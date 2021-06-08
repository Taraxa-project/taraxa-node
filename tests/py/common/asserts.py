def EQ(*vals):
    if not vals:
        return
    first = vals[0]
    for val in vals:
        assert val == first
