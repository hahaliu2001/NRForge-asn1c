#!/bin/sh
set -eu
# Test-only transient file; the production renderer returns text only.
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-cpp.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
./check_asn1typed_render_cpp > "$work/generated.cpp"
cat "$work/generated.cpp"
# CXX may include configured compiler arguments, as in Automake recipes.
printf '%s\n' "${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic -fsyntax-only $work/generated.cpp"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic -fsyntax-only "$work/generated.cpp"
printf '%s\n' 'PASS generated C++20 compilation (exit 0)'
