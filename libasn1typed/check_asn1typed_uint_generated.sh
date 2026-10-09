#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-n5.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
mkdir "$work/main" "$work/renamed"
./check_asn1typed_uint_render "$src/fixtures/uint-generation-n5.asn1" UIntGeneration uinttest "$work/main"
./check_asn1typed_uint_render "$src/fixtures/uint-generation-renamed-n5.asn1" UIntRenamed renameduint "$work/renamed"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_uint_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
