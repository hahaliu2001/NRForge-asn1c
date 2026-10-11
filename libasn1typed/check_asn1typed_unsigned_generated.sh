#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-e1p2.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
sed 's/ProcedureCode ::= INTEGER (0..255)/ProcedureCode ::= INTEGER (0..18446744073709551615)/' "$src/fixtures/envelope-evidence-n11.asn1" > "$work/envelope.asn1"
./check_asn1typed_unsigned_render "$src/fixtures/unsigned-integers-e1p2.asn1" "$work" "$work/envelope.asn1"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_unsigned_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
