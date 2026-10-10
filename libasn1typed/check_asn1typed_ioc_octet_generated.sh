#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-ioc-octets.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
./check_asn1typed_ioc_render "$src/fixtures/ioc-octets-n14.asn1" IOCOctets Message octets_main "$work/main"
./check_asn1typed_ioc_render "$src/fixtures/ioc-octets-n14.asn1" IOCOctets ExtensionMessage octets_ext "$work/ext"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_ioc_octet_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
