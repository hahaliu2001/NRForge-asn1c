#!/bin/sh
set -eu
# Test-only transient output; production renderer returns owned text only.
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-ioc-cpp.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
./check_asn1typed_ioc_cpp > "$work/generated.cpp"
cat "$work/generated.cpp"
printf '%s\n' "${CXX:-g++} -std=c++20 -Wall -Wextra -Werror -pedantic -fsyntax-only $work/generated.cpp"
${CXX:-g++} -std=c++20 -Wall -Wextra -Werror -pedantic -fsyntax-only "$work/generated.cpp"
printf '%s\n' 'PASS actual production IOC generated C++20 compilation (exit 0)'
