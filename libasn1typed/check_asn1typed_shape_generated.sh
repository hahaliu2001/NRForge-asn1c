#!/bin/sh
set -eu
src=${srcdir:-.}
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-shape.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
./check_asn1typed_shape_render "$src/fixtures/shape-generation-n17.asn1" ShapeGeneration shapetest "$work"
./check_asn1typed_ioc_render "$src/fixtures/ioc-shapes-n17.asn1" IOCShapeGeneration Message iocshapes "$work/physical"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG -I"$src/../libaper" -I"$work" "$src/check_asn1typed_shape_generated.cpp" "$src/../libaper/runtime.cpp" -o "$work/check"
"$work/check"
