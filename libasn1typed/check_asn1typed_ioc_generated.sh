#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-n9p4.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
src=${srcdir:-.}
for specification in 'DispatchMessage main' 'EmptyMessage empty' 'ClosedMessage closed' 'ExtensionMessage ext' 'EmptyExtensionMessage empty_ext'; do
    set -- $specification
    ./check_asn1typed_ioc_render "$src/fixtures/ioc-generation-n9.asn1" IOCCppGeneration "$1" "foo::nrforge::$2" "$work/$2"
done
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_ioc_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
