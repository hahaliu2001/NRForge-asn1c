"""Installed finite RRC SDK: independent semantics, bits, conversion and ownership."""

from concurrent.futures import ThreadPoolExecutor
import copy
import gc
import itertools
import json
from pathlib import Path
import subprocess
import sys
import unittest

import nrforge_rrc as sdk

DATA = json.loads((Path(__file__).parent / "golden.json").read_text())
CASES = DATA["cases"]


def unpack(v):
    if type(v) is dict and set(v) == {"bytes"}:
        return bytes.fromhex(v["bytes"])
    if type(v) is dict and set(v) == {"repeat", "count"}:
        return [copy.deepcopy(unpack(v["repeat"])) for _ in range(v["count"])]
    if type(v) is dict:
        return {k: unpack(x) for k, x in v.items()}
    if type(v) is list:
        return [unpack(x) for x in v]
    return v


def slots(schema, value, typ="T0", path=()):
    declarations = {d["name"]: d for d in schema["types"]}
    if value is None:
        return
    if typ.startswith("std::optional<"):
        yield from slots(schema, value, typ[14:-1], path)
    elif typ.startswith("std::vector<") and typ != "std::vector<std::byte>":
        for i, item in enumerate(value):
            yield from slots(schema, item, typ[12:-1], path + (i,))
    elif typ in declarations:
        d = declarations[typ]
        if d["kind"] == "scalar":
            yield from slots(schema, value, d["type"], path)
        elif d["kind"] == "sequence":
            for field in d["fields"]:
                yield path + (field["name"],), field
                if field["name"] in value:
                    yield from slots(
                        schema,
                        value[field["name"]],
                        field["type"],
                        path + (field["name"],),
                    )
        elif d["kind"] == "choice" and value["type"] != "@unknown":
            a = next(x for x in d["alternatives"] if x["tag"] == value["type"])
            yield from slots(schema, value["value"], a["type"], path + ("value",))


def parent(value, path):
    for k in path[:-1]:
        value = value[k]
    return value


class SDKTests(unittest.TestCase):
    def mib(self):
        return next(c for c in CASES if c["root"] == "MIB")

    def encode(self, c, value=None, **kw):
        return sdk.encode(
            c["module"], c["root"], unpack(c["value"]) if value is None else value, **kw
        )

    def decode(self, c, data=None, **kw):
        return sdk.decode(
            c["module"],
            c["root"],
            bytes.fromhex(c["wire_hex"]) if data is None else data,
            **kw
        )

    def test_identity_catalog_and_schema(self):
        self.assertEqual(sdk.identity(), DATA["sdk_identity"])
        self.assertEqual(len(sdk.roots()), 64)
        self.assertEqual(
            {(r["module"], r["root"]) for r in sdk.roots()},
            {(c["module"], c["root"]) for c in CASES},
        )
        manifest = sdk.manifest()
        self.assertEqual(len(manifest["roots"]), 85)
        self.assertFalse(manifest["full_rrc_coverage"])
        self.assertEqual(manifest["qualification_sha256"], DATA["qualification_sha256"])
        for r in manifest["roots"]:
            if r["status"] != "PASS":
                with self.assertRaises(ValueError):
                    sdk.encode(r["module"], r["root"], {})
                with self.assertRaises(ValueError):
                    sdk.schema(r["module"], r["root"])
        c = self.mib()
        schema = sdk.schema(c["module"], c["root"])
        schema["types"].clear()
        self.assertTrue(sdk.schema(c["module"], c["root"])["types"])

    def test_all_independent_real_cases_and_completions(self):
        self.assertEqual(len(CASES), 143)
        for c in CASES:
            with self.subTest(root=c["root"], case=c["id"]):
                expected, wire, n = (
                    unpack(c["expected"]),
                    bytes.fromhex(c["wire_hex"]),
                    c["semantic_bits"],
                )
                for completion in ("octets", "contained_bits", "exact"):
                    encoded = self.encode(c, completion=completion)
                    self.assertEqual(
                        encoded,
                        dict(
                            octets=wire,
                            semantic_bits=n,
                            encoded_bits=len(wire) * 8 if completion == "octets" else n,
                            padding_bits=(-n) % 8 if completion == "octets" else 0,
                            empty_substitution=False,
                        ),
                    )
                    decoded = self.decode(
                        c,
                        logical_bits=None if completion == "octets" else n,
                        completion=completion,
                    )
                    self.assertEqual(decoded, dict(value=expected, semantic_bits=n))
        print("RRC_REAL_CASES 143 ROOTS 64 COMPLETIONS 3")

    def test_native_optional_false(self):
        self.assertEqual(
            {c["id"] for c in DATA["supplemental"]},
            {"optional-boolean", "optional-enumerated", "optional-choice"},
        )
        for c in DATA["supplemental"]:
            if c["id"] != "optional-boolean":
                continue
            with self.subTest(case=c["id"]):
                self.assertEqual(self.encode(c)["octets"], bytes.fromhex(c["wire_hex"]))
                self.assertEqual(self.decode(c)["value"], unpack(c["expected"]))
                value = unpack(c["expected"])
                schema = sdk.schema(c["module"], c["root"])
                changed = False
                for path, field in slots(schema, value):
                    if field["optional"]:
                        p = parent(value, path)
                        if p[path[-1]] is False or p[path[-1]] == {"present": True}:
                            p[path[-1]] = None
                            self.assertNotEqual(
                                self.encode(c, value)["octets"],
                                bytes.fromhex(c["wire_hex"]),
                            )
                            changed = True
                            break
                self.assertTrue(changed)

    def test_default_omission_materializes_effective_values(self):
        seen = set()
        for c in CASES:
            if c["id"] != "mandatory-low":
                continue
            value = unpack(c["expected"])
            schema = sdk.schema(c["module"], c["root"])
            for path, field in slots(schema, value):
                if field["default"] is not None:
                    changed = copy.deepcopy(value)
                    del parent(changed, path)[path[-1]]
                    self.assertEqual(
                        self.encode(c, changed)["octets"], bytes.fromhex(c["wire_hex"])
                    )
                    self.assertEqual(self.decode(c)["value"], value)
                    seen.add((c["root"], path))
        self.assertGreaterEqual(len(seen), 5)
        print("RRC_DEFAULT_OMISSIONS", len(seen))

    def test_shapes_storage_overflow_and_bool_rejection(self):
        c = self.mib()
        original = unpack(c["value"])
        for v in (True, -1, 1 << 63, -(1 << 63) - 1, 1 << 100):
            body = copy.deepcopy(original)
            body["ssb-SubcarrierOffset"] = v
            with self.assertRaises((TypeError, ValueError, OverflowError)):
                self.encode(c, body)
        for change in (
            lambda d: d.update(extra=0),
            lambda d: d.pop("systemFrameNumber"),
            lambda d: d.update(systemFrameNumber={"octets": b"\0", "bit_count": True}),
        ):
            body = copy.deepcopy(original)
            change(body)
            with self.assertRaises((TypeError, ValueError, OverflowError)):
                self.encode(c, body)

        class D(dict):
            pass

        with self.assertRaises(TypeError):
            self.encode(c, D(original))
        for kw in (
            dict(logical_bits=True),
            dict(logical_bits=-1),
            dict(logical_bits=1 << 100),
            dict(completion="unknown"),
            dict(limits={"other": 1}),
            dict(conversion_limits={"max_nodes": True}),
        ):
            with self.assertRaises((TypeError, ValueError, OverflowError)):
                self.decode(c, **kw)
        with self.assertRaises(TypeError):
            self.decode(c, bytearray.fromhex(c["wire_hex"]))

    def test_truncation_padding_trailing_and_codec_quotas(self):
        checks = 0
        for c in CASES:
            wire, n = bytes.fromhex(c["wire_hex"]), c["semantic_bits"]
            for cut in range(len(wire)):
                with self.assertRaises(sdk.CodecError):
                    self.decode(c, wire[:cut])
                checks += 1
            for cut in (0, n - 1):
                with self.assertRaises(sdk.CodecError):
                    self.decode(c, logical_bits=cut, completion="contained_bits")
                checks += 1
            with self.assertRaises(sdk.CodecError) as error:
                self.decode(c, limits={"max_depth": 0})
            self.assertEqual(error.exception.code, "resource_limit")
            self.assertIs(type(error.exception.bit_offset), int)
            checks += 1
            with self.assertRaises(sdk.CodecError):
                self.decode(c, wire + b"\0")
            checks += 1
            if n % 8:
                changed = wire[:-1] + bytes([wire[-1] | 1])
                with self.assertRaises(sdk.CodecError) as error:
                    self.decode(c, changed)
                self.assertEqual(error.exception.code, "nonzero_padding")
                checks += 1
        print("RRC_MALFORMED_QUOTA_CHECKS", checks)

    def test_independent_conversion_and_codec_limits(self):
        c = self.mib()
        for limits in ({"max_depth": 0}, {"max_nodes": 0}, {"max_bytes": 0}):
            with self.assertRaisesRegex(ValueError, "conversion resource limit"):
                self.encode(c, conversion_limits=limits)
            with self.assertRaisesRegex(ValueError, "conversion resource limit"):
                self.decode(c, conversion_limits=limits)
        for limits in (
            {"max_output_octets": 0},
            {"max_wire_bits": 0},
            {"max_depth": 0},
            {"max_work_bits": 0},
        ):
            with self.assertRaises(sdk.CodecError):
                self.encode(c, limits=limits)
        with self.assertRaises(sdk.CodecError):
            self.decode(c, limits={"max_input_octets": 0})
        body = unpack(c["value"])
        body["pdcch-ConfigSIB1"] = body
        with self.assertRaises((TypeError, ValueError)):
            self.encode(c, body, conversion_limits={"max_depth": 16})

    def test_owned_lifetime_and_independent_results(self):
        c = self.mib()
        data = bytes.fromhex(c["wire_hex"])
        result = self.decode(c, data)
        del data
        gc.collect()
        self.assertEqual(result["value"], unpack(c["expected"]))
        result["value"]["pdcch-ConfigSIB1"]["controlResourceSetZero"] = 1
        fresh = self.decode(c)
        self.assertEqual(fresh["value"], unpack(c["expected"]))
        owned = self.encode(c, result["value"])
        result.clear()
        gc.collect()
        self.assertEqual(
            self.decode(c, owned["octets"])["value"]["pdcch-ConfigSIB1"][
                "controlResourceSetZero"
            ],
            1,
        )

    def test_unknown_enumeration_extension(self):
        c = next(c for c in CASES if c["root"] == "FilterConfig")
        value = unpack(c["value"])
        value["filterCoefficientRSRP"] = {"extension_index": 1000}
        encoded = self.encode(c, value)
        self.assertEqual(
            self.decode(c, encoded["octets"])["value"]["filterCoefficientRSRP"],
            {"extension_index": 1000},
        )
        for n in (True, -1, 1 << 64):
            value["filterCoefficientRSRP"] = {"extension_index": n}
            with self.assertRaises((TypeError, ValueError, OverflowError)):
                self.encode(c, value)

    def test_native_unknown_choice_and_enum_semantics(self):
        for c in DATA["supplemental"]:
            if c["id"] == "optional-boolean":
                continue
            with self.subTest(case=c["id"]):
                encoded = self.encode(c)
                self.assertEqual(encoded["octets"], bytes.fromhex(c["wire_hex"]))
                self.assertEqual(encoded["semantic_bits"], c["semantic_bits"])
                self.assertEqual(self.decode(c)["value"], unpack(c["expected"]))

    def test_concurrent_owned_calls(self):
        def run(c):
            value = unpack(c["value"])
            encoded = self.encode(c, value)
            decoded = self.decode(c, encoded["octets"])
            return decoded == dict(
                value=unpack(c["expected"]), semantic_bits=c["semantic_bits"]
            )

        with ThreadPoolExecutor(max_workers=4) as pool:
            self.assertTrue(all(pool.map(run, CASES)))

    def test_all_four_protocol_import_orders(self):
        modules = ("nrforge_rrc", "nrforge_ngap", "nrforge_f1ap", "nrforge_e1ap")
        for order in itertools.permutations(modules):
            script = "\n".join("import " + name for name in order)
            script += "\nimport nrforge_rrc as r,nrforge_ngap as n,nrforge_f1ap as f,nrforge_e1ap as e\n"
            script += "assert [len(n.messages()),len(f.messages()),len(e.messages()),len(r.roots())]==[131,158,72,64]\n"
            script += (
                "assert len({r.CodecError,n.CodecError,f.CodecError,e.CodecError})==4\n"
            )
            script += "d=r.decode('NR-RRC-Definitions','MIB',bytes.fromhex('020008'));assert d['semantic_bits']==23\n"
            script += "assert r.encode('NR-RRC-Definitions','MIB',d['value'])['octets']==bytes.fromhex('020008')\n"
            subprocess.run(
                [sys.executable, "-I", "-c", script],
                check=True,
                capture_output=True,
                text=True,
            )
        print("RRC_FOUR_PROTOCOL_IMPORT_ORDERS 24")


if __name__ == "__main__":
    unittest.main()
