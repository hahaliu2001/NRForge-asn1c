#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-private.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
./check_asn1typed_private_render "$src/fixtures/private-generation-batch.asn1" PrivateKeys Message privatekeys "$work/private"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_private_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
