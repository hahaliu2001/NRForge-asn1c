#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-n8p3.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
./check_asn1typed_collection_render "$src/fixtures/collection-generation-n8.asn1" CollectionGeneration collectiontest "$work"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_collection_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
