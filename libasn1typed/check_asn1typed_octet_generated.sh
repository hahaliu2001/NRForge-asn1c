#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-n14.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
./check_asn1typed_octet_render "$src/fixtures/octets-generation-n14.asn1" "$work"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_octet_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
