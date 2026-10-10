#!/bin/sh
set -eu
src=${srcdir:-.}
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-null.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
./check_asn1typed_null_render "$src/fixtures/null-generation-batch.asn1" "$work" "$src/fixtures/ioc-null-batch.asn1"
./check_asn1typed_ioc_render "$src/fixtures/ioc-null-batch.asn1" IOCNullGeneration Message iocnull "$work/physical"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_null_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
