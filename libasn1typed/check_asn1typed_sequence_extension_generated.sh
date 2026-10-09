#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-n7p3.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
mkdir "$work/main" "$work/compat" "$work/reversed"
./check_asn1typed_sequence_extension_render "$src/fixtures/sequence-extension-generation-n7.asn1" SequenceExtensionGeneration extensiontest "$work/main"
./check_asn1typed_sequence_extension_render "$src/fixtures/compound-generation-n6.asn1" CompoundGeneration compoundtest "$work/compat" compat
./check_asn1typed_sequence_extension_render "$src/fixtures/compound-generation-reversed-n6.asn1" CompoundReversed reversedcompound "$work/reversed" compat
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_sequence_extension_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
