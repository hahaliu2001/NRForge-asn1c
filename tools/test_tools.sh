#!/bin/sh
set -eu
probe=${1:-./asn1typed_real_probe}
inspect=${2:-./asn1typed_tree_inspect}
fixture_root=${3:-../libasn1typed/fixtures}
ioc_probe=${4:-./asn1typed_ioc_probe}
tmp=${TMPDIR:-/tmp}/asn1typed-tools-$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"
printf '%s\n' "$fixture_root/ioc-t3.asn1" > "$tmp/ioc.modules"
printf '%s\n' "$fixture_root/missing.asn1" > "$tmp/missing.modules"
printf '%s\n' "$fixture_root/ordinary-t2.asn1" > "$tmp/ordinary.modules"
printf '%s\n' "$fixture_root/parameterized-reference-b7a.asn1" > "$tmp/parameterized.modules"
if "$probe" >/dev/null 2>&1; then exit 1; fi
if "$probe" --module-list "$tmp/no-list" --root-module SyntheticIOC --message Registration >"$tmp/out" 2>&1; then exit 1; fi
grep -q 'PARSE FAIL' "$tmp/out"
if "$probe" --module-list "$tmp/missing.modules" --root-module SyntheticIOC --message Registration >"$tmp/out" 2>&1; then exit 1; fi
grep -q 'PARSE FAIL' "$tmp/out"
if "$probe" --module-list "$tmp/ioc.modules" --root-module Missing --message Registration >"$tmp/out"; then exit 1; fi
grep -q 'EXTRACT FAIL' "$tmp/out"
if "$probe" --module-list "$tmp/ioc.modules" --root-module SyntheticIOC --message Missing >"$tmp/out"; then exit 1; fi
grep -q 'EXTRACT FAIL' "$tmp/out"
"$probe" --module-list "$tmp/ioc.modules" --root-module SyntheticIOC --message Registration >"$tmp/out"
grep -q '^EXTRACT PASS$' "$tmp/out"
grep -q '^OWNED_TYPES ' "$tmp/out"
if "$inspect" >/dev/null 2>&1; then exit 1; fi
if "$inspect" --module-list "$tmp/ordinary.modules" --type Missing.PagingDRX >"$tmp/out"; then exit 1; fi
grep -q 'LOOKUP FAIL MODULE' "$tmp/out"
if "$inspect" --module-list "$tmp/ordinary.modules" --type OrdinaryTypes.Missing >"$tmp/out"; then exit 1; fi
grep -q 'LOOKUP FAIL TYPE' "$tmp/out"
if "$inspect" --module-list "$tmp/ordinary.modules" --type OrdinaryTypes.Person --member missing >"$tmp/out"; then exit 1; fi
grep -q 'LOOKUP FAIL MEMBER' "$tmp/out"
"$inspect" --module-list "$tmp/ordinary.modules" --type OrdinaryTypes.PagingDRX >"$tmp/out"
grep -q '^TYPE PagingDRX$' "$tmp/out"
grep -q '^DECLARED_CONSTRAINT NONE$' "$tmp/out"
grep -q '^COMBINED_CONSTRAINT NONE$' "$tmp/out"
"$inspect" --module-list "$tmp/ordinary.modules" --type OrdinaryTypes.Person --member age >"$tmp/out"
grep -q '^MEMBER age$' "$tmp/out"
"$inspect" --module-list "$tmp/ordinary.modules" --type OrdinaryTypes.BoundedPersonList >"$tmp/out"
grep -q '^DECLARED_CONSTRAINT$' "$tmp/out"
grep -q '^COMBINED_CONSTRAINT$' "$tmp/out"
grep -q 'ValueRange' "$tmp/out"
"$inspect" --module-list "$tmp/parameterized.modules" --type ParameterizedReferenceB7A.UseA --member selected >"$tmp/out"
grep -q '^RHS_PARAMETER_COUNT 1$' "$tmp/out"
grep -q 'CONTAINED_SUBTYPE TYPE REFERENCE SetA' "$tmp/out"
printf '%s\n' "$fixture_root/ioc-generation-n9.asn1" > "$tmp/physical.modules"
if "$ioc_probe" >/dev/null 2>&1; then exit 1; fi
"$ioc_probe" --module-list "$tmp/physical.modules" --root-module IOCCppGeneration --message DispatchMessage --output-prefix "$tmp/physical" >"$tmp/out"
grep -q '^PARSER_DELETED$' "$tmp/out"
grep -q '^GENERATION codec PASS$' "$tmp/out"
test -s "$tmp/physical_types.hpp"
test -s "$tmp/physical_mapping.hpp"
test -s "$tmp/physical_codec.hpp"
"$ioc_probe" --module-list "$tmp/physical.modules" --root-module IOCCppGeneration --message DispatchMessage --output-prefix "$tmp/deterministic" --verify-determinism >"$tmp/out"
grep -q '^DETERMINISM types PASS$' "$tmp/out"
grep -q '^DETERMINISM mapping PASS$' "$tmp/out"
grep -q '^DETERMINISM codec PASS$' "$tmp/out"
cmp "$tmp/physical_types.hpp" "$tmp/deterministic_types.hpp"
cmp "$tmp/physical_mapping.hpp" "$tmp/deterministic_mapping.hpp"
cmp "$tmp/physical_codec.hpp" "$tmp/deterministic_codec.hpp"
if "$ioc_probe" --module-list "$tmp/physical.modules" --root-module IOCCppGeneration --message DispatchMessage --namespace class --output-prefix "$tmp/refused" >"$tmp/out"; then exit 1; fi
grep -q '^GENERATION types FAIL$' "$tmp/out"
test ! -e "$tmp/refused_types.hpp"
# Envelope CLI guards run before parsing and cannot overwrite the body files.
set +e
"$ioc_probe" --module-list "$tmp/no-list" --root-module IOCCppGeneration --message DispatchMessage --output-prefix "$tmp/physical" --envelope-module Missing >"$tmp/out" 2>&1
status=$?
set -e
test "$status" -eq 2
grep -q '^Usage:' "$tmp/out"
set +e
"$ioc_probe" --module-list "$tmp/no-list" --root-module IOCCppGeneration --message DispatchMessage --output-prefix "$tmp/physical" --envelope-module Missing --envelope-type Missing --envelope-output-prefix "$tmp/physical" >"$tmp/out" 2>&1
status=$?
set -e
test "$status" -eq 2
cmp "$tmp/physical_types.hpp" "$tmp/deterministic_types.hpp"
set +e
"$ioc_probe" --module-list "$tmp/physical.modules" --root-module IOCCppGeneration --message DispatchMessage --output-prefix "$tmp/no-body" --envelope-module Missing --envelope-type Missing --envelope-output-prefix "$tmp/no-envelope" >"$tmp/out" 2>&1
status=$?
set -e
test "$status" -eq 5
grep -q '^ENVELOPE EXTRACT FAIL$' "$tmp/out"
test ! -e "$tmp/no-body_types.hpp"
test ! -e "$tmp/no-envelope_types.hpp"
