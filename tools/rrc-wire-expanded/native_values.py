"""Finite native semantic profiles; no generated model/codec input."""

import copy


def endpoint(constraint, high=False, default=0):
    if constraint is None or not constraint.root:
        return default
    values = []
    for item in constraint.root:
        if isinstance(item, int):
            values.append(item)
        else:
            value = item.ub if high else item.lb
            if value is None:
                raise ValueError("unbounded native endpoint")
            values.append(value)
    return max(values) if high else min(values)


def build(obj, high=False, depth=0):
    if depth > 64:
        raise ValueError("native profile depth")
    kind = obj.TYPE
    if kind == "INTEGER":
        return endpoint(obj._const_val, high)
    if kind == "BOOLEAN":
        return high
    if kind == "NULL":
        return 0
    if kind == "ENUMERATED":
        return obj._root[-1 if high else 0]
    if kind in ("BIT STRING", "OCTET STRING"):
        if kind == "OCTET STRING" and getattr(obj, "_const_cont", None) is not None:
            child = obj._const_cont
            child.set_val(build(child, high, depth + 1))
            return child.to_uper()
        n = endpoint(obj._const_sz)
        if n > 65536:
            raise ValueError("native profile size")
        return (
            (((1 << n) - 1 if high else 0), n)
            if kind == "BIT STRING"
            else bytes([255 if high else 0]) * n
        )
    if kind == "SEQUENCE OF":
        n = endpoint(obj._const_sz)
        if n > 64:
            raise ValueError("native profile collection lower bound")
        return [build(obj._cont, high, depth + 1) for _ in range(n)]
    if kind == "CHOICE":
        name = obj._root[-1 if high else 0]
        return name, build(obj._cont[name], high, depth + 1)
    if kind == "SEQUENCE":
        return {
            name: build(obj._cont[name], high, depth + 1)
            for name in obj._root_mand
            + (
                [n for n in obj._root_opt if obj._cont[n]._def is not None]
                if high
                else []
            )
        }
    raise ValueError(("native profile unsupported kind", kind))


def choice_variants(obj, value, path=()):
    """One replacement per CHOICE reached in mandatory baseline; no cartesian blowup."""
    if obj.TYPE == "CHOICE":
        for name in obj._root:
            yield path, (name, build(obj._cont[name])), obj._name + "." + name
        yield from choice_variants(obj._cont[value[0]], value[1], path + (1,))
    elif obj.TYPE == "SEQUENCE":
        for name, item in value.items():
            yield from choice_variants(obj._cont[name], item, path + (name,))
    elif obj.TYPE == "SEQUENCE OF":
        for i, item in enumerate(value):
            yield from choice_variants(obj._cont, item, path + (i,))


def replace(value, path, new):
    if not path:
        return new
    if isinstance(value, tuple):
        parts = list(value)
        parts[path[0]] = replace(parts[path[0]], path[1:], new)
        return tuple(parts)
    value = copy.deepcopy(value)
    value[path[0]] = replace(value[path[0]], path[1:], new)
    return value


def pack(value):
    if isinstance(value, bytes):
        return {"bytes": value.hex()}
    if isinstance(value, tuple):
        return {"tuple": [pack(v) for v in value]}
    if isinstance(value, list):
        if len(value) > 8 and all(v == value[0] for v in value):
            return {"repeat": pack(value[0]), "count": len(value)}
        return [pack(v) for v in value]
    if isinstance(value, dict):
        return {k: pack(v) for k, v in value.items()}
    return value


def restore(value):
    if isinstance(value, dict) and set(value) == {"bytes"}:
        return bytes.fromhex(value["bytes"])
    if isinstance(value, dict) and set(value) == {"tuple"}:
        return tuple(restore(v) for v in value["tuple"])
    if isinstance(value, dict) and set(value) == {"repeat", "count"}:
        return [copy.deepcopy(restore(value["repeat"])) for _ in range(value["count"])]
    if isinstance(value, dict):
        return {k: restore(v) for k, v in value.items()}
    if isinstance(value, list):
        return [restore(v) for v in value]
    return value


def carrier_projection(obj, value):
    """Public native Contents decode may return inner semantics; project only
    its canonical byte carrier, without reading any C++/IR wire implementation.
    The qualified input profile uses canonical native-produced inner bytes.
    """
    if obj.TYPE == "OCTET STRING" and isinstance(value, tuple):
        child = obj._const_cont
        if child is None: raise ValueError("unexpected native OCTET tuple")
        child.set_val(copy.deepcopy(value[1]))
        return child.to_uper()
    if obj.TYPE == "SEQUENCE":
        return {name: carrier_projection(obj._cont[name], item) for name, item in value.items()}
    if obj.TYPE == "CHOICE":
        if value[0].startswith("_ext_"): return value
        return value[0], carrier_projection(obj._cont[value[0]], value[1])
    if obj.TYPE == "SEQUENCE OF": return [carrier_projection(obj._cont, item) for item in value]
    return copy.deepcopy(value)
