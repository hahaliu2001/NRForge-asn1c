"""Finite native-vector and conversion safety checks for an installed wheel.

Run: python -m unittest discover -s /absolute/path/to/python/tests -v
No source tree or schema is used by the installed nrforge_ngap module.
"""
import copy
from concurrent.futures import ThreadPoolExecutor
import hashlib
import importlib.util
import json
from pathlib import Path
import unittest

import nrforge_ngap as ngap

HERE = Path(__file__).resolve().parent
GOLDEN = json.loads((HERE / "golden-131.json").read_text())["cases"]
SETUP = json.loads((HERE / "ng-setup-native.json").read_text())["cases"]


def payload(entry):
    return entry["value"]["value"]["value"]


class SDKTests(unittest.TestCase):
    def test_identity_and_registry(self):
        identity = ngap.identity()
        self.assertEqual(identity["version"], "0.1.0")
        for key in ("fingerprint", "schema_sha256", "runtime_sha256"):
            self.assertRegex(identity[key], r"^[0-9a-f]{64}$")
        rows = ngap.messages()
        self.assertEqual(len(rows), 131)
        self.assertEqual({r["message"] for r in rows}, {r["message"] for r in GOLDEN})
        self.assertTrue(ngap.schema("NGSetupRequest"))

    def test_all_131_native_vectors(self):
        self.assertEqual(len(GOLDEN), 131)
        for case in GOLDEN:
            with self.subTest(message=case["message"]):
                wire = bytes.fromhex(case["pdu_hex"])
                self.assertEqual(hashlib.sha256(wire).hexdigest(), case["wire_sha256"])
                decoded = ngap.decode(wire)
                self.assertEqual(decoded["kind"], "typed")
                self.assertEqual(decoded["message"], case["message"])
                self.assertEqual(decoded["procedure_code"], case["procedure_code"])
                self.assertEqual(decoded["criticality"], case["criticality"])
                self.assertEqual(decoded["role"], case["root"])
                self.assertEqual(ngap.encode(case["message"], decoded["body"],
                                            criticality=decoded["criticality"]), wire)

    def test_concurrent_calls_have_independent_contexts(self):
        # Each worker owns its input and decoded tree. Repeat different messages
        # so a shared cursor/context or body would corrupt bytes or metadata.
        chosen = [GOLDEN[0], GOLDEN[2], GOLDEN[40], GOLDEN[-1]]
        def worker(case):
            wire = bytes.fromhex(case["pdu_hex"])
            for _ in range(8):
                decoded = ngap.decode(wire)
                if decoded["message"] != case["message"]:
                    raise AssertionError("Concurrent dispatch returned another message")
                encoded = ngap.encode(case["message"], decoded["body"],
                                      criticality=case["criticality"])
                if encoded != wire:
                    raise AssertionError("Concurrent encode differs from native bytes")
                # An error in this call must not poison the next complete call.
                try:
                    ngap.decode(b"")
                except ngap.CodecError as error:
                    if (error.code, error.bit_offset) != ("truncated_input", 0):
                        raise AssertionError("Concurrent error differs from first error")
                else:
                    raise AssertionError("Empty input unexpectedly succeeded")
            return case["message"]
        with ThreadPoolExecutor(max_workers=4) as pool:
            result = list(pool.map(worker, chosen))
        self.assertEqual(result, [case["message"] for case in chosen])

    def test_independent_ng_setup_fields(self):
        for case in SETUP:
            with self.subTest(message=case["message"]):
                wire = bytes.fromhex(case["pdu_hex"])
                decoded = ngap.decode(wire)
                body = decoded["body"]
                entries = body["protocol_i_es"]["elements"]
                self.assertEqual(body["sequence_extensions"],
                                 {"received_bitmap_bit_count": 0, "unknown_additions": []})
                if case["message"] == "NGSetupRequest":
                    self.assertEqual([e["id"] for e in entries], [27, 102, 21])
                    node = payload(entries[0])["value"]["value"]
                    self.assertEqual(node["p_lmn_identity"], b"\0" * 3)
                    self.assertEqual(node["g_nb_id"]["value"]["value"],
                                     {"octets": b"\0" * 3, "bit_count": 22})
                    self.assertIsNone(node["i_e_extensions"])
                    ta = payload(entries[1])["elements"][0]
                    self.assertEqual(ta["t_ac"], b"\0" * 3)
                    plmn = ta["broadcast_plmn_list"]["elements"][0]
                    self.assertEqual(plmn["p_lmn_identity"], b"\0" * 3)
                    slice_ = plmn["t_ai_slice_support_list"]["elements"][0]["s_nssai"]
                    self.assertEqual(slice_["s_st"], b"\0")
                    self.assertIsNone(slice_["s_d"])
                    self.assertEqual(payload(entries[2]), "v_32")
                elif case["message"] == "NGSetupResponse":
                    self.assertEqual([e["id"] for e in entries], [1, 96, 86, 80])
                    self.assertEqual(payload(entries[0]), "A")
                    guami = payload(entries[1])["elements"][0]["g_uami"]
                    self.assertEqual(guami["p_lmn_identity"], b"\0" * 3)
                    for field, bits, octets in (("a_mf_region_id", 8, 1),
                                                ("a_mf_set_id", 10, 2),
                                                ("a_mf_pointer", 6, 1)):
                        self.assertEqual(guami[field], {"octets": b"\0" * octets, "bit_count": bits})
                    self.assertEqual(payload(entries[2]), 0)
                    self.assertEqual(payload(entries[3])["elements"][0]["p_lmn_identity"], b"\0" * 3)
                else:
                    self.assertEqual([e["id"] for e in entries], [15])
                    cause = payload(entries[0])
                    self.assertEqual(cause["type"], "NgapIEsCause_radio_network")
                    self.assertEqual(cause["value"], {"value": "unspecified"})
                self.assertEqual(ngap.encode(case["message"], body,
                                            criticality=case["criticality"]), wire)

    def request(self):
        case = next(c for c in SETUP if c["message"] == "NGSetupRequest")
        return ngap.decode(bytes.fromhex(case["pdu_hex"]))["body"]

    def test_example_constructs_request(self):
        path = HERE.parent / "examples" / "ng_setup.py"
        spec = importlib.util.spec_from_file_location("ng_setup_example", path)
        example = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(example)
        wire = ngap.encode("NGSetupRequest", example.request_body(), criticality="reject")
        self.assertEqual(wire.hex(), next(c["pdu_hex"] for c in SETUP if c["message"] == "NGSetupRequest"))

    def test_strict_python_shapes(self):
        for invalid in (True, -1, 1 << 100):
            body = self.request()
            body["protocol_i_es"]["elements"][0]["id"] = invalid
            with self.subTest(invalid=invalid), self.assertRaises((TypeError, ValueError, OverflowError)):
                ngap.encode("NGSetupRequest", body)
        body = self.request()
        body["invented"] = 1
        with self.assertRaises((TypeError, ValueError)):
            ngap.encode("NGSetupRequest", body)
        body = self.request()
        del body["protocol_i_es"]
        with self.assertRaises((TypeError, ValueError)):
            ngap.encode("NGSetupRequest", body)
        body = self.request()
        body["protocol_i_es"]["elements"][0]["value"]["type"] = "wrong_wrapper"
        with self.assertRaises((TypeError, ValueError)):
            ngap.encode("NGSetupRequest", body)
        body = self.request()
        bitstring = payload(body["protocol_i_es"]["elements"][0])["value"]["value"]["g_nb_id"]["value"]["value"]
        bitstring["bit_count"] = True
        with self.assertRaises((TypeError, ValueError)):
            ngap.encode("NGSetupRequest", body)
        body = self.request()
        payload(body["protocol_i_es"]["elements"][0])["value"]["value"]["p_lmn_identity"] = "000000"
        with self.assertRaises((TypeError, ValueError)):
            ngap.encode("NGSetupRequest", body)
        with self.assertRaises((TypeError, ValueError)):
            ngap.encode("NotAnNGAPMessage", {})

    def test_rejects_custom_container_dispatch_and_large_labels(self):
        class ComputedDict(dict):
            def __getitem__(self, key):
                return copy.deepcopy(super().__getitem__(key))
        class IteratorList(list):
            def __iter__(self):
                raise RuntimeError("Custom iteration must not run during conversion")
        class CustomKey(str):
            pass
        body = self.request()
        for invalid in (ComputedDict(body), {CustomKey(k): v for k, v in body.items()}):
            with self.subTest(container=type(invalid).__name__), self.assertRaises((TypeError, ValueError)):
                ngap.encode("NGSetupRequest", invalid)
        body = self.request()
        body["protocol_i_es"]["elements"] = IteratorList(body["protocol_i_es"]["elements"])
        with self.assertRaises((TypeError, ValueError)):
            ngap.encode("NGSetupRequest", body)
        with self.assertRaises((TypeError, ValueError)):
            ngap.encode("N" * 257, {})
        body = self.request()
        body["protocol_i_es"]["elements"][0]["value"]["type"] = "W" * 257
        with self.assertRaises((TypeError, ValueError)):
            ngap.encode("NGSetupRequest", body)
        body = self.request()
        body["protocol_i_es"]["elements"][0]["criticality"] = "E" * 257
        with self.assertRaises((TypeError, ValueError)):
            ngap.encode("NGSetupRequest", body)
        with self.assertRaises((TypeError, ValueError)):
            ngap.encode("NGSetupRequest", self.request(), conversion_limits={"max_bytes": 1})

    def test_unknown_ie_and_enum_retained(self):
        body = self.request()
        unknown = {"id": 65535, "criticality": "ignore",
                   "value": {"type": "NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_unknown",
                             "value": {"payload": b"\x00\xff\x80"}}}
        # Independent APER: original mandatory fields, IE count 4, then
        # id=65535, ignore, open-type length 3, opaque bytes 00 ff 80.
        wire = bytes.fromhex("0015002c000004001b000800000000000000000066000d000000000000000000000000000015400100ffff400300ff80")
        decoded = ngap.decode(wire)
        self.assertEqual(decoded["body"]["protocol_i_es"]["elements"][-1], unknown)
        # Inherited C++ contract retains unknown IOC payloads on receive only.
        with self.assertRaises(ngap.CodecError) as raised:
            ngap.encode("NGSetupRequest", decoded["body"])
        self.assertEqual(raised.exception.code, "constraint_violation")
        body = self.request()
        body["protocol_i_es"]["elements"][2]["value"]["value"]["value"] = {"extension_index": 77}
        wire = ngap.encode("NGSetupRequest", body)
        decoded = ngap.decode(wire)
        self.assertEqual(payload(decoded["body"]["protocol_i_es"]["elements"][2]),
                         {"extension_index": 77})
        self.assertEqual(ngap.encode("NGSetupRequest", decoded["body"]), wire)

    def test_unknown_outer_payloads_retained(self):
        opaque = ngap.decode(bytes.fromhex("40fa4001a5"))
        self.assertEqual(opaque["kind"], "opaque_root")
        self.assertEqual(opaque["procedure_code"], 250)
        self.assertEqual(opaque["role"], "unsuccessfulOutcome")
        self.assertEqual(opaque["criticality"], "ignore")
        self.assertEqual(opaque["payload"], b"\xa5")
        extension = ngap.decode(bytes.fromhex("8001aa"))
        self.assertEqual(extension["kind"], "unknown_extension")
        self.assertEqual(extension["extension_index"], 0)
        self.assertEqual(extension["payload"], b"\xaa")

    def test_sequence_extension_and_optional_retention(self):
        # Independent APER: outer open type length 42; body extension bit=1;
        # original mandatory fields; normally-small bitmap length-minus-one 2
        # (7 bits), bitmap 001, zero align, open-type length 2, bytes 00 ff.
        wire = bytes.fromhex("0015002a800003001b000800000000000000000066000d00000000000000000000000000001540010004400200ff")
        decoded = ngap.decode(wire)
        self.assertEqual(decoded["body"]["sequence_extensions"], {
            "received_bitmap_bit_count": 3,
            "unknown_additions": [{"addition_index": 2, "payload_octets": b"\x00\xff"}],
        })
        # Unknown SEQUENCE additions are retained, never silently re-emitted.
        with self.assertRaises(ngap.CodecError) as raised:
            ngap.encode("NGSetupRequest", decoded["body"])
        self.assertEqual(raised.exception.code, "constraint_violation")
        # Missing OPTIONAL fields and explicit None represent the same absent value.
        body = self.request()
        node = payload(body["protocol_i_es"]["elements"][0])["value"]["value"]
        del node["i_e_extensions"]
        self.assertEqual(ngap.encode("NGSetupRequest", body),
                         ngap.encode("NGSetupRequest", self.request()))

    def test_conversion_budgets(self):
        wire = bytes.fromhex(SETUP[0]["pdu_hex"])
        for key in ("max_depth", "max_nodes", "max_bytes", "max_collection_elements"):
            with self.subTest(key=key), self.assertRaises((ValueError, ngap.CodecError)):
                ngap.decode(wire, conversion_limits={key: 0})
        with self.assertRaises((TypeError, ValueError)):
            ngap.decode(wire, conversion_limits={"invented": 1})

    def test_errors_and_budgets(self):
        case = next(c for c in SETUP if c["message"] == "NGSetupRequest")
        wire = bytes.fromhex(case["pdu_hex"])
        for data in (b"", wire[:-1], wire + b"\0"):
            with self.subTest(data=data.hex()), self.assertRaises(ngap.CodecError) as raised:
                ngap.decode(data)
            self.assertIsInstance(raised.exception.code, str)
            self.assertIsInstance(raised.exception.bit_offset, int)
        with self.assertRaises(ngap.CodecError) as raised:
            ngap.decode(b"")
        self.assertEqual((raised.exception.code, raised.exception.bit_offset), ("truncated_input", 0))
        with self.assertRaises(ngap.CodecError) as raised:
            ngap.decode(wire + b"\0")
        self.assertEqual((raised.exception.code, raised.exception.bit_offset), ("trailing_data", len(wire) * 8))
        with self.assertRaises(ngap.CodecError) as raised:
            ngap.decode(wire, limits={"max_input_octets": len(wire) - 1})
        self.assertEqual(raised.exception.code, "resource_limit")
        self.assertEqual(ngap.decode(wire, limits={"max_input_octets": len(wire)})["message"], case["message"])
        for limits in ({"max_output_octets": len(wire) - 1}, {"max_wire_bits": len(wire) * 8 - 1}):
            with self.subTest(limits=limits), self.assertRaises(ngap.CodecError) as raised:
                ngap.encode(case["message"], self.request(), limits=limits)
            self.assertEqual(raised.exception.code, "resource_limit")
        for invalid in ({"max_input_octets": -1}, {"max_wire_bits": True}, {"invented": 1}):
            with self.subTest(limits=invalid), self.assertRaises((TypeError, ValueError, OverflowError)):
                ngap.decode(wire, limits=invalid)


if __name__ == "__main__":
    unittest.main()
