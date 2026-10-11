#!/usr/bin/env python3
"""Pinned independent UPER codecs; no NRForge code in native encoding."""
import asn1tools
from asn1tools.codecs.uper import Encoder
import argparse, contextlib, hashlib, importlib.util, io, json, pathlib, subprocess, tempfile
from importlib.metadata import version
from pycrate_asn1c.asnproc import compile_text
from pycrate_asn1c.generator import PycrateGenerator


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--report", required=True)
    args = p.parse_args()
    root = pathlib.Path(__file__).resolve().parents[2]
    assert version("pycrate") == "0.7.11"
    assert version("asn1tools") == "0.167.0"
    vectors = []
    declarations = []

    def add(kind, offset, lo, hi, general, ext, count, value, asn, native):
        name = f"T{len(vectors)}"
        prefix = ", ".join(f"p{i} BOOLEAN" for i in range(offset))
        declarations.append(
            f"{name} ::= SEQUENCE {{ "
            + (prefix + ", " if prefix else "")
            + f"v {asn}"
            + " }"
        )
        val = {f"p{i}": True for i in range(offset)}
        val["v"] = native
        vectors.append((name, kind, offset, lo, hi, general, ext, count, value, val))

    for offset in range(8):
        for upper in (
            0,
            1,
            2,
            254,
            255,
            256,
            65535,
            65536,
            2**39 - 1,
            2**63 - 1,
            2**64 - 1,
        ):
            for v in sorted({0, upper // 2, upper}):
                add(
                    "U",
                    offset,
                    0,
                    upper if upper < 2**63 else -1,
                    False,
                    False,
                    0,
                    str(v),
                    f"INTEGER (0..{upper})",
                    v,
                )
        for v in (7, 8, 135, 263):
            add("U", offset, 7, 263, False, False, 0, str(v), "INTEGER (7..263)", v)
        for v in (-123, -1, 0, 234):
            add(
                "I",
                offset,
                -123,
                234,
                False,
                False,
                0,
                str(v),
                "INTEGER (-123..234)",
                v,
            )
        for v in (-(2**63), -129, -128, -1, 0, 127, 128, 2**63 - 1):
            add(
                "I",
                offset,
                -(2**63),
                2**63 - 1,
                False,
                False,
                0,
                str(v),
                "INTEGER (-9223372036854775808..9223372036854775807)",
                v,
            )
            add("N", offset, 0, 0, True, False, 0, str(v), "INTEGER", v)
            add("X", offset, -4, 4, False, True, 0, str(v), "INTEGER (-4..4, ...)", v)
        for v in (0, 1, 3, 4, 128, 255, 2**63, 2**64 - 1):
            add("V", offset, 1, 3, False, True, 0, str(v), "INTEGER (1..3, ...)", v)
        enum = (
            "ENUMERATED { a, b, c, ..., " + ", ".join(f"e{i}" for i in range(70)) + " }"
        )
        for v in (0, 1, 2):
            add("A", offset, 0, 3, True, False, 0, str(v), enum, "abc"[v])
        for v in (0, 1, 63, 64, 65, 69):
            add("A", offset, 0, 3, True, True, 0, str(v), enum, f"e{v}")
        for kind in ("O", "B"):
            for n in (
                0,
                1,
                2,
                16,
                127,
                128,
                16383,
                16384,
                16385,
                65535,
                65536,
                65537,
                131072,
            ):
                data = bytes([0xA5]) * ((n + 7) // 8 if kind == "B" else n)
                if kind == "B" and n % 8:
                    data = data[:-1] + bytes([data[-1] & (255 << (8 - n % 8))])
                native = (
                    (int.from_bytes(data, "big") >> (len(data) * 8 - n), n)
                    if kind == "B"
                    else data
                )
                add(
                    kind,
                    offset,
                    0,
                    0,
                    True,
                    False,
                    n,
                    data.hex() or "-",
                    "BIT STRING" if kind == "B" else "OCTET STRING",
                    native,
                )
            for n in (0, 1, 3, 4, 9):
                data = bytes([0xA5]) * ((n + 7) // 8 if kind == "B" else n)
                if kind == "B" and n % 8:
                    data = data[:-1] + bytes([data[-1] & (255 << (8 - n % 8))])
                native = (
                    (int.from_bytes(data, "big") >> (len(data) * 8 - n), n)
                    if kind == "B"
                    else data
                )
                add(
                    kind,
                    offset,
                    1,
                    3,
                    False,
                    True,
                    n,
                    data.hex() or "-",
                    ("BIT STRING" if kind == "B" else "OCTET STRING")
                    + " (SIZE (1..3, ...))",
                    native,
                )
            for n in (0, 1, 15, 16):
                data = bytes(n if kind == "O" else (n + 7) // 8)
                add(
                    kind,
                    offset,
                    0,
                    16,
                    False,
                    False,
                    n,
                    data.hex() or "-",
                    ("OCTET STRING" if kind == "O" else "BIT STRING")
                    + " (SIZE (0..16))",
                    data if kind == "O" else (0, n),
                )
            for n in (1, 16, 65535, 65536):
                data = bytes(n if kind == "O" else (n + 7) // 8)
                add(
                    kind,
                    offset,
                    n,
                    n,
                    False,
                    False,
                    n,
                    data.hex() or "-",
                    ("OCTET STRING" if kind == "O" else "BIT STRING")
                    + f" (SIZE ({n}))",
                    data if kind == "O" else (0, n),
                )
        for n in (1, 63, 64, 65, 127):
            data = (1 << (((n + 7) // 8) * 8 - n)).to_bytes((n + 7) // 8, "big")
            asn = (
                "SEQUENCE { root NULL, ..., "
                + ", ".join(f"e{i} BOOLEAN OPTIONAL" for i in range(n))
                + " }"
            )
            add(
                "Q",
                offset,
                0,
                0,
                False,
                False,
                n,
                data.hex(),
                asn,
                {"root": None, f"e{n-1}": False},
            )
        for n in (0, 1, 127, 128, 16384, 65536):
            payload = bytes([0xA5]) * n
            # Inner unconstrained OCTET STRING is itself complete, then open framed.
            inner = (
                bytes([n]) + payload
                if n < 128
                else (
                    (bytes([0x80 | (n >> 8), n & 255]) + payload)
                    if n < 16384
                    else bytes([0xC0 + n // 16384]) + payload + b"\0"
                )
            )
            add(
                "P",
                offset,
                0,
                0,
                False,
                False,
                n,
                inner.hex(),
                "CHOICE { root NULL, ..., extra OCTET STRING }",
                ("extra", payload),
            )
    schema = (
        "Probe DEFINITIONS AUTOMATIC TAGS ::= BEGIN\n"
        + "\n".join(declarations)
        + "\nEND"
    )
    with tempfile.TemporaryDirectory(prefix="nrforge-uper-") as tmp:
        tmp = pathlib.Path(tmp)
        with contextlib.redirect_stdout(io.StringIO()):
            compile_text(schema)
            PycrateGenerator(str(tmp / "native.py"))
        spec = importlib.util.spec_from_file_location("native", tmp / "native.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        driver = tmp / "probe"
        subprocess.run(
            [
                "c++",
                "-std=c++20",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I" + str(root / "libuper"),
                str(root / "libuper/runtime.cpp"),
                str(root / "tools/uper-qualification/probe.cpp"),
                "-o",
                str(driver),
            ],
            check=True,
        )
        native = asn1tools.compile_string(schema, "uper")
        requests = []
        expected = []
        records = []
        for name, kind, off, lo, hi, g, ext, n, value, val in vectors:
            native_val = dict(val)
            if kind == "B":
                data = bytes.fromhex(value) if value != "-" else b""
                native_val["v"] = (data, n)
            asn1tools_available = not (
                (kind == "B" and ext) or (kind == "P" and n >= 16384)
            )
            pycrate_available = not (
                (kind == "B" and n >= 16384 and (g or hi >= 65536))
                or (kind == "Q" and n > 64)
            )
            assert asn1tools_available or pycrate_available
            wire = None
            bits = None
            if asn1tools_available:
                enc = Encoder()
                native.types[name]._type.encode(native_val, enc)
                bits = enc.number_of_bits + enc.chunks_number_of_bits
                wire = bytes(enc.as_bytearray()) or b"\0"
                assert native.decode(name, wire) == native_val, (
                    name,
                    "asn1tools decode",
                    native.decode(name, wire),
                    native_val,
                )
            if pycrate_available:
                obj = getattr(module.Probe, name)
                pycrate_val = dict(val)
                if kind == "Q":
                    pycrate_val["v"] = {
                        key: (0 if key == "root" else False) for key in val["v"]
                    }
                obj.set_val(pycrate_val)
                alternate = obj.to_uper_ws()
                alternate_bits = sum(c.get_bl() for c in obj._struct if c._name != "P")
                if wire is None:
                    wire = alternate
                    bits = alternate_bits
                else:
                    assert (alternate, alternate_bits) == (wire, bits), (
                        name,
                        "native encoders disagree",
                        alternate.hex()[:100],
                        alternate_bits,
                        wire.hex()[:100],
                        bits,
                    )
                obj.from_uper(wire)
                assert obj.get_val() == pycrate_val, (name, "pycrate decode")
            head = f"{kind} {off} {lo} {hi} {int(g)} {int(ext)} {n} {value}"
            requests.extend([f"E {head} -", f"D {head} {wire.hex()}"])
            expected.extend([f"{wire.hex()} {bits}", f"{value} {bits}"])
            records.append(
                {
                    "type": name,
                    "kind": kind,
                    "offset": off,
                    "size": n,
                    "value_sha256": hashlib.sha256(value.encode()).hexdigest(),
                    "wire_sha256": hashlib.sha256(wire).hexdigest(),
                    "wire_octets": len(wire),
                    "semantic_bits": bits,
                    "pycrate_available": pycrate_available,
                    "asn1tools_available": asn1tools_available,
                }
            )
        result = subprocess.run(
            [str(driver)],
            input="\n".join(requests) + "\n",
            text=True,
            capture_output=True,
            check=True,
        )
        lines = result.stdout.splitlines()
        assert len(lines) == len(expected)
        for i, (actual, want) in enumerate(zip(lines, expected)):
            assert actual == want, (
                i // 2,
                vectors[i // 2][:8],
                actual[:100],
                want[:100],
            )
        report = {
            "status": "PASS",
            "oracle": "asn1tools",
            "oracle_version": version("asn1tools"),
            "second_oracle": "pycrate",
            "second_oracle_version": version("pycrate"),
            "standard": "ITU-T X.691 (02/2021)",
            "vectors": len(vectors),
            "comparisons": len(expected),
            "native_decode_pass": len(records),
            "asn1tools_comparisons": sum(r["asn1tools_available"] for r in records),
            "pycrate_comparisons": sum(r["pycrate_available"] for r in records),
            "oracle_limitation": "pycrate fragmented BIT STRING and bitmap >64 defects use asn1tools; asn1tools extensible BIT STRING and fragmented open types use pycrate; bitmap >127 has focused normative checks only; every recorded row has independent encode/decode, no oracle patched",
            "schema_sha256": hashlib.sha256(schema.encode()).hexdigest(),
            "records": records,
            "source_sha256": {
                str(f.relative_to(root)): hashlib.sha256(f.read_bytes()).hexdigest()
                for f in [
                    root / "libuper/runtime.hpp",
                    root / "libuper/runtime.cpp",
                    pathlib.Path(__file__).resolve(),
                    root / "tools/uper-qualification/probe.cpp",
                ]
            },
        }
        pathlib.Path(args.report).write_text(json.dumps(report, indent=2) + "\n")
        print(
            f"UPER independent oracle PASS: {len(vectors)} vectors, {len(expected)} comparisons"
        )


if __name__ == "__main__":
    main()
