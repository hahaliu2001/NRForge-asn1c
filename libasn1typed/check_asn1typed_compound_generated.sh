#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-n6.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
mkdir "$work/main" "$work/reversed"
./check_asn1typed_compound_render "$src/fixtures/compound-generation-n6.asn1" CompoundGeneration compoundtest "$work/main"
./check_asn1typed_compound_render "$src/fixtures/compound-generation-reversed-n6.asn1" CompoundReversed reversedcompound "$work/reversed"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_compound_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
