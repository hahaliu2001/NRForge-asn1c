#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-n16.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
./check_asn1typed_integer_render "$src/fixtures/integers-generation-n16.asn1" IntegerGeneration integertests "$work"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_integer_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
