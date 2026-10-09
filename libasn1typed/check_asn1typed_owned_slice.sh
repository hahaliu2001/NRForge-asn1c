#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-s1.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
./check_asn1typed_owned_slice > "$work/generated.cpp"
cat >> "$work/generated.cpp" <<'CPP'
#include <cassert>
#include <type_traits>
using namespace nrforge::synthetic::cpp_aper_slice;
static_assert(std::is_copy_constructible_v<Selection> && std::is_copy_assignable_v<Selection>);
static_assert(std::is_copy_constructible_v<Packet> && std::is_copy_assignable_v<Packet>);
static_assert(std::is_same_v<Count, std::uint64_t>);
static_assert(Count_constraint::lower_bound == 0);
static_assert(Count_constraint::upper_bound == 65535);
static_assert(!std::is_same_v<Selection_flag, Selection_count>);
static_assert(nrforge::synthetic::cpp_aper_int64::Wide_constraint::lower_bound == INT64_MIN);
static_assert(nrforge::synthetic::cpp_aper_int64::Wide_constraint::upper_bound == INT64_MAX);
static_assert(std::is_same_v<nrforge::synthetic::cpp_aper_int64::Wide, std::int64_t>);
int main() {
    Selection f = Selection_flag{false};
    Selection c = Selection_count{Count{65535}};
    auto fcopy = f; auto ccopy = c;
    assert(std::get<Selection_flag>(fcopy).value == false);
    assert(std::get<Selection_count>(ccopy).value == 65535);
    Packet p{}; p.count = 65535; p.selection = Selection_count{p.count};
    assert(!p.enabled.has_value());
    p.enabled = false; assert(p.enabled.has_value() && !*p.enabled);
    p.enabled = true; assert(p.enabled.has_value() && *p.enabled);
    Packet q = p; assert(q.enabled == p.enabled && q.count == 65535);
    assert(nrforge::synthetic::cpp_aper_int64::Wide_constraint::lower_bound == INT64_MIN);
}
CPP
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -pedantic "$work/generated.cpp" -o "$work/check"
"$work/check"
echo 'PASS owned synthetic C++20 generated type compilation and value semantics'
