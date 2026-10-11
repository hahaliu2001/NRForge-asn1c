"""Qualification-only native values -> independently constructed public E1AP BODY.

Reuse the established NGAP semantic walker, not a production codec or wire parser.
Only the public E1AP registry API performs encoding/decoding in emitted tests.
Private mapping text supplies public wrapper type names and IOC key-to-row identity;
it is never used to obtain expected values or expected wire bytes.
"""
import importlib.util
from pathlib import Path
import re


REUSED_BRIDGE_PATH = Path(__file__).resolve().parents[1] / 'unified-ngap-qualification' / 'semantic_bridge.py'
_spec = importlib.util.spec_from_file_location('_nrforge_e1ap_reused_bridge', REUSED_BRIDGE_PATH)
_module = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_module)


class Bridge(_module.Bridge):
    """Fail-closed semantic walker with E1AP public full-PDU emission."""

    def walk(self, desc, value, typ, p, c):
        # Pycrate names unknown ENUMERATED additions _ext_N. Their independent
        # addition index is a semantic value, not an ASN.1 assigned integer.
        if desc.TYPE == 'ENUMERATED' and isinstance(value, str) and re.fullmatch(r'_ext_[0-9]+', value):
            index = int(value[5:])
            if 'UnknownExtension' not in self.structs[self.bare(typ)]:
                raise ValueError(('unknown addition in closed ENUMERATED', typ, value))
            addition = self.q(typ) + '::UnknownExtension'
            self.lines.append(f'{p}.value = {addition}{{UINT64_C({index})}};')
            self.checks.extend([f'REQUIRE(::std::holds_alternative<{addition}>({c}.value));',
                                f'REQUIRE(::std::get<{addition}>({c}.value).index == UINT64_C({index}));'])
            return
        if desc.TYPE in ('IA5String', 'NumericString', 'GeneralString'):
            self.assign_scalar(p, c, value)
            return
        # The parent walker dispatches recursively through self.walk/open.
        # INTEGER (including SRBID's noncontiguous ASN.1 constraint), inline
        # constructed CHOICE, ordered collections, optional presence, named
        # ENUMERATED additions, BIT STRING width, IOC id and criticality are
        # consequently compared at every selected leaf, not by roundtrip only.
        return super().walk(desc, value, typ, p, c)

    def emit_unified(self, desc, cases, cpp_body_type, public_header, index,
                     message, role, code, module='E1AP-PDU-Contents'):
        """Emit one public-header-only TU defining run_identity_NNN().

        Cases contain id/value/pdu_hex/criticality. The native oracle produces
        pdu_hex separately; the walker constructs BODY solely from value. No
        generated adapter or private mapping/codec header is compiled here.
        Link emitted objects with the complete generated E1AP registry archive.
        """
        functions = []
        for n, case in enumerate(cases):
            if case['criticality'] not in ('reject', 'ignore', 'notify'):
                raise ValueError(('invalid root criticality', case['criticality']))
            if 'root' in case and {'initiatingMessage': 0, 'successfulOutcome': 1,
                                   'unsuccessfulOutcome': 2}.get(case['root']) != role:
                raise ValueError(('case root disagrees with message', message, case['root']))
            if 'procedure' in case and case['procedure'] != code:
                raise ValueError(('case procedure disagrees with message', message, case['procedure']))
            # Fail rather than silently generating invalid C++ or ambiguous receipts.
            if not re.fullmatch(r'[A-Za-z0-9_.:-]+', str(case['id'])):
                raise ValueError(('unsafe case receipt id', case['id']))
            self.reset()
            self.walk(desc, case['value'], cpp_body_type, 'body', 'actual_body')
            policy = case['criticality']
            wire = self.literal(bytes.fromhex(case['pdu_hex']))
            functions += [f'static void case_{n}() {{', 'using namespace ::nrforge::e1ap;',
                          f'{self.q(cpp_body_type)} body{{}};', *self.lines,
                          f'auto expected = make_e1ap_pdu(::std::move(body), Criticality::{policy});',
                          'REQUIRE(expected);', f'const auto native_wire = {wire};',
                          'auto decoded = decode_e1ap_pdu(native_wire);', 'REQUIRE(decoded);',
                          'REQUIRE(decoded.value().kind() == PduKind::typed);',
                          'const auto* header = decoded.value().root_header();',
                          f'REQUIRE(header && static_cast<unsigned>(header->role) == {role} && header->procedure_code == {code} && header->received_criticality == Criticality::{policy});',
                          'const auto* metadata = decoded.value().message_info();',
                          f'REQUIRE(metadata && metadata->message == "{message}" && metadata->module == "{module}");',
                          f'const auto* actual_body_pointer = decoded.value().body_if<{self.q(cpp_body_type)}>();',
                          'REQUIRE(actual_body_pointer);', 'const auto& actual_body = *actual_body_pointer;',
                          *self.checks,
                          'auto encoded = encode_e1ap_pdu(expected.value());', 'REQUIRE(encoded);',
                          'REQUIRE(encoded.value().octets.size() == native_wire.size());',
                          'REQUIRE(encoded.value().octets == native_wire);',
                          f'::std::cout << "CASE {index} {case["id"]} ";',
                          'for(auto octet:encoded.value().octets) { const auto v=::std::to_integer<unsigned>(octet); ::std::cout << "0123456789abcdef"[v>>4] << "0123456789abcdef"[v&15]; }',
                          '::std::cout << "\\n";', '}']
        if not cases:
            raise ValueError(('no native cases for message', message))
        includes = ['#include "e1ap.hpp"', f'#include "{public_header}"',
                    '#include <cstdlib>', '#include <iostream>', '#include <type_traits>',
                    '#define REQUIRE(...) do { if(!(__VA_ARGS__)) { ::std::cerr << "semantic failure " << __LINE__ << "\\n"; ::std::abort(); } } while(0)']
        return '\n'.join([*includes, *functions, f'void run_identity_{index:03d}() {{',
                          *[f'case_{n}();' for n in range(len(cases))], '}']) + '\n'
