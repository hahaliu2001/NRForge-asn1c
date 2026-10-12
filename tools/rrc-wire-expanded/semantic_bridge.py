"""Qualification-only native semantic values -> public owned UPER models.

Only model declarations/source metadata are read. No codec body, wire parsing or
renderer functions supply expected semantic values, bytes or bit counts.
"""

import json
import re


def blocks(text):
    out = {}
    for m in re.finditer(r"\bstruct\s+(T\d+)\s*\{", text):
        pos = m.end()
        start = pos
        depth = 1
        while depth:
            depth += (text[pos] == "{") - (text[pos] == "}")
            pos += 1
        out[m[1]] = text[start : pos - 1]
    return out


def literal(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, int):
        return (
            str(v) + "ULL"
            if v >= 0
            else ("(-9223372036854775807LL-1)" if v == -(1 << 63) else str(v) + "LL")
        )
    if isinstance(v, bytes):
        return (
            "std::vector<std::byte>{"
            + ",".join("std::byte{0x%02x}" % x for x in v)
            + "}"
        )
    raise ValueError(type(v))


class Bridge:
    def __init__(self, text):
        self.models = blocks(text)
        self.ns = re.search(r"namespace ([\w:]+) \{", text)[1]
        self.lines = []
        self.serial = 0

    def scalar(self, obj, value, p):
        kind = obj.TYPE
        if kind in ("INTEGER", "BOOLEAN", "OCTET STRING"):
            self.lines.append(f"{p}={literal(value)};")
        elif kind == "NULL":
            self.lines.append(f"{p}=std::monostate{{}};")
        elif kind == "BIT STRING":
            n, width = value
            raw = (n << ((-width) % 8)).to_bytes((width + 7) // 8, "big")
            self.lines.extend(
                [f"{p}.octets={literal(raw)};", f"{p}.bit_count={width};"]
            )
        elif kind == "ENUMERATED":
            if value.startswith("_ext_"):
                ext = True
                index = int(value[5:])
            else:
                ext = value not in obj._root
                names = (
                    obj._ext if ext else sorted(obj._root, key=lambda n: obj._cont[n])
                )
                index = names.index(value)
            self.lines.append(
                f"{p}=nrforge::uper::Index{{{str(ext).lower()},{index}}};"
            )
        else:
            raise ValueError(("bridge unsupported scalar", kind))

    def walk(self, obj, value, typ, p):
        if not typ.startswith("T"):
            self.scalar(obj, value, p)
            return
        body = self.models[typ]
        kind = obj.TYPE
        if kind in (
            "INTEGER",
            "BOOLEAN",
            "NULL",
            "OCTET STRING",
            "BIT STRING",
            "ENUMERATED",
        ):
            self.scalar(obj, value, p + ".value")
            return
        if kind == "SEQUENCE OF":
            element = re.search(r"std::vector<([^>]+)> value", body)[1]
            self.lines.append(f"{p}.value.resize({len(value)});")
            if len(value) > 8 and all(v == value[0] for v in value):
                self.serial += 1
                local = "element" + str(self.serial)
                self.lines.append(
                    f"g::{element} {local}{{}};"
                    if element.startswith("T")
                    else f"{element} {local}{{}};"
                )
                self.walk(obj._cont, value[0], element, local)
                self.lines.append(
                    f"std::fill({p}.value.begin(),{p}.value.end(),{local});"
                )
            else:
                for i, item in enumerate(value):
                    self.walk(obj._cont, item, element, f"{p}.value[{i}]")
            return
        if kind == "CHOICE":
            names = {
                json.loads(s): int(i)
                for i, s in re.findall(r'alternative(\d+)=("[^"\n]*")', body)
            }
            if value[0].startswith("_ext_"):
                self.lines.append(
                    f"{p}.value=g::Unknown{{{int(value[0][5:])},{literal(value[1])}}};"
                )
                return
            i = names[value[0]]
            element = re.search(r"struct A" + str(i) + r"\s*\{\s*([^;]+) value", body)[
                1
            ].strip()
            self.serial += 1
            local = "chosen" + str(self.serial)
            self.lines.append(f"g::{typ}::A{i} {local}{{}};")
            self.walk(obj._cont[value[0]], value[1], element, local + ".value")
            self.lines.append(f"{p}.value=std::move({local});")
            return
        if kind == "SEQUENCE":
            names = [
                json.loads(s)
                for _, s in re.findall(r'field_source(\d+)=("[^"\n]*")', body)
            ]
            fields = re.findall(r"^([^ \n]+) (f_\w+)\s*[{;=]", body, re.M)
            if len(names) != len(fields):
                raise ValueError(("bridge fields mismatch", typ, names, fields))
            for name, (ft, field) in zip(names, fields):
                fp = p + "." + field
                if name.startswith("@group"):
                    slot = int(name[6:])
                    members = obj._ext_nest[slot]
                    group_id = next(
                        i for i, names in obj._ext_group.items() if names == members
                    )
                    desc = obj._ext_group_obj[group_id]
                    item = {n: value[n] for n in members if n in value}
                    present = bool(item)
                else:
                    desc = obj._cont[name]
                    present = name in value
                    item = value.get(name, desc._def)
                if ft.startswith("std::optional<"):
                    if not present:
                        continue
                    ft = ft[len("std::optional<") : -1]
                    self.lines.append(f"{fp}.emplace();")
                    fp = "(*" + fp + ")"
                elif item is None and not present:
                    raise ValueError(("bridge mandatory missing", name))
                self.walk(desc, item, ft, fp)
            return
        raise ValueError(("bridge unsupported constructed", kind))
