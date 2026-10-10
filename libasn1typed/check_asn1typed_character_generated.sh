#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-characters.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
./check_asn1typed_character_render "$src/fixtures/characters-generation-batch.asn1" "$work"
./check_asn1typed_ioc_render "$src/fixtures/ioc-characters-generation-batch.asn1" IOCCharacterGeneration Message ioccharacters "$work/physical"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_character_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
