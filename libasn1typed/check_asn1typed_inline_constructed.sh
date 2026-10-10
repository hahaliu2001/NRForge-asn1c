#!/bin/sh
set -eu
src=${srcdir:-.}
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-inline.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
./check_asn1typed_inline_constructed "$src/fixtures/inline-constructed-f1p2.asn1" "$work"
mkdir "$work/physical"
./check_asn1typed_inline_constructed "$src/fixtures/inline-constructed-f1p2.asn1" "$work/physical" physical
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_inline_constructed.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
