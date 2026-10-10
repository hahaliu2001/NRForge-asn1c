#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-domains.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
./check_asn1typed_domain_render "$src/fixtures/extensible-integers-batch.asn1" IntegerDomains domains "$work"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_domain_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
