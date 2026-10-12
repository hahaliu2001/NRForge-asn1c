"""Finite optional witnesses selected from independent native descriptors."""
import copy
from native_values import build, endpoint

REQUESTS = [
    ("NR-RRC-Definitions", "RRCSetup", "singleton-srb", "SRB-Identity-v1700"),
    ("NR-RRC-Definitions", "SidelinkUEInformationNR-r16", "integer-elements", "SL-InterestedFreqList-r16"),
    ("NR-RRC-Definitions", "LoggedMeasurementConfiguration-r16", "bit32-elements", "BIT32_LIST"),
    ("PC5-RRC-Definitions", "RRCReconfigurationSidelink", "setup-release", "SL-DRX-ConfigUC-r17"),
    ("NR-RRC-Definitions", "RRCReconfigurationComplete", "opaque-self-carrier", "SELF_CARRIER"),
    ("NR-RRC-Definitions", "UEInformationResponse-r16", "alias-size16", "MobilityHistoryReport-r16"),
    ("NR-RRC-Definitions", "LocationMeasurementIndication", "known-null-open", "NULL_ADDITION"),
]

def matches(obj, target, parent=None, name=None):
    ref = getattr(obj, "_typeref", None)
    if target == "BIT32_LIST":
        return obj.TYPE == "SEQUENCE OF" and obj._cont.TYPE == "BIT STRING" and endpoint(obj._cont._const_sz) == 32 and endpoint(obj._cont._const_sz, True) == 32
    if target == "SELF_CARRIER":
        c = getattr(obj, "_const_cont", None)
        return obj.TYPE == "OCTET STRING" and c is not None and c._typeref.called == ("NR-RRC-Definitions", "RRCReconfigurationComplete")
    if target == "NULL_ADDITION":
        return obj.TYPE == "NULL" and parent is not None and parent.TYPE == "CHOICE" and name in (parent._ext or [])
    return ref is not None and ref.called[1] == target


def find(obj, target):
    seen = set()
    def walk(o, path, parent=None, name=None):
        if id(o) in seen: return None
        seen.add(id(o))
        if path and matches(o, target, parent, name): return path, o
        if o.TYPE in ("SEQUENCE", "CHOICE"):
            for n, child in o._cont.items():
                result = walk(child, path + [n], o, n)
                if result: return result
        elif o.TYPE == "SEQUENCE OF": return walk(o._cont, path + [0], o, 0)
        return None
    result = walk(obj, [])
    if result is None: raise ValueError(("native witness path unavailable", obj._name, target))
    return result


def place(obj, path, value):
    if not path: return copy.deepcopy(value)
    head, rest = path[0], path[1:]
    if obj.TYPE == "CHOICE": return head, place(obj._cont[head], rest, value)
    if obj.TYPE == "SEQUENCE":
        out = build(obj)
        for gid, members in getattr(obj, "_ext_group", {}).items():
            if head in members: out.update(build(obj._ext_group_obj[gid], True))
        out[head] = place(obj._cont[head], rest, value)
        return out
    if obj.TYPE == "SEQUENCE OF":
        out = build(obj)
        if not out: out = [build(obj._cont)]
        out[head] = place(obj._cont, rest, value)
        return out
    raise ValueError(("native witness path type", obj.TYPE, path))


def selected(obj, feature, target):
    path, desc = find(obj, target)
    value = build(desc)
    if feature in ("integer-elements", "bit32-elements"):
        count = min(2, endpoint(desc._const_sz, True))
        if count < 1: raise ValueError("witness list unavailable")
        value = [build(desc._cont), build(desc._cont, True)][:count]
    elif feature == "alias-size16":
        if endpoint(desc._const_sz) != 1 or endpoint(desc._const_sz, True) != 16: raise ValueError("alias bounds drift")
        value = [build(desc._cont, True) for _ in range(16)]
    elif feature == "singleton-srb":
        if value != 4: raise ValueError("singleton drift")
    return place(obj, path, value), dict(feature=feature, path=path, native_type=desc.TYPE, native_reference=list(desc._typeref.called) if getattr(desc, "_typeref", None) else None)
