#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-n4.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
mkdir "$work/main" "$work/renamed"
./check_asn1typed_enum_render "$src/fixtures/enum-generation-n4.asn1" EnumGeneration enumtest "$work/main"
./check_asn1typed_enum_render "$src/fixtures/enum-generation-renamed-n4.asn1" EnumRenamed renamedtest "$work/renamed"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_enum_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
