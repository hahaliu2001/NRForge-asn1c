#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-n11p2.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
./check_asn1typed_envelope_render "$src/fixtures/ioc-generation-n9.asn1" "$src/fixtures/envelope-evidence-n11.asn1" "$work/normal" "$work/upper"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_envelope_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
