import dataclasses


def to_dict(dataclass_inst, nonexistence_value=None):
    ret = {}
    for field in dataclasses.fields(dataclass_inst):
        val = getattr(dataclass_inst, field.name)
        if val != nonexistence_value:
            ret[field.name] = val
    return ret
