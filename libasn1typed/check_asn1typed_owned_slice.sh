#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-s1.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
./check_asn1typed_owned_slice > "$work/generated.cpp"
cat >> "$work/generated.cpp" <<'CPP'
#include <cassert>
#include <type_traits>
#include <utility>
using namespace nrforge::synthetic::cpp_aper_slice;
static_assert(std::is_copy_constructible_v<Selection> && std::is_copy_assignable_v<Selection>);
static_assert(std::is_copy_constructible_v<Packet> && std::is_copy_assignable_v<Packet>);
static_assert(std::is_same_v<Count, std::uint64_t>);
static_assert(Count_constraint::lower_bound == 0);
static_assert(Count_constraint::upper_bound == 65535);
static_assert(Count_aper::value_bit_width == 16 && Count_aper::align_before_payload_to_octet);
static_assert(Count_aper::most_significant_octet_first && !Count_aper::has_length_determinant);
static_assert(Count_aper::lower_bound == 0 && Count_aper::upper_bound == 65535);
static_assert(APER_BOOLEAN::value_bit_width == 1);
static_assert(APER_BOOLEAN::false_bit == 0 && APER_BOOLEAN::true_bit == 1);
static_assert(!APER_BOOLEAN::align_before_payload_to_octet);
static_assert(Selection_aper::selector_bit_width == 1);
static_assert(!Selection_aper::selector_align_before_payload_to_octet);
static_assert(Selection_aper::storage_alternative_to_per_root_index[0] == 0);
static_assert(Selection_aper::storage_alternative_to_per_root_index[1] == 1);
static_assert(Selection_aper::per_root_index_to_storage_ordinal[0] == 0);
static_assert(Selection_aper::per_root_index_to_storage_ordinal[1] == 1);
static_assert(Selection_aper::storage_alternative_to_per_root_index[Selection_aper::per_root_index_to_storage_ordinal[0]] == 0);
static_assert(Selection_aper::storage_alternative_to_per_root_index[Selection_aper::per_root_index_to_storage_ordinal[1]] == 1);
static_assert(std::is_same_v<Selection_aper::count_payload_mapping, Count_aper>);
static_assert(std::is_same_v<Selection_aper::storage_ordinal_0_wrapper, std::variant_alternative_t<0, Selection>>);
static_assert(std::is_same_v<Selection_aper::storage_ordinal_0_wrapper, Selection_flag>);
static_assert(std::is_same_v<decltype(std::declval<Selection_flag>().value), Selection_aper::storage_ordinal_0_payload_type>);
static_assert(std::is_same_v<Selection_aper::storage_ordinal_0_payload_type, bool>);
static_assert(std::is_same_v<Selection_aper::storage_ordinal_0_payload_mapping, APER_BOOLEAN>);
static_assert(std::is_same_v<Selection_aper::storage_ordinal_1_wrapper, std::variant_alternative_t<1, Selection>>);
static_assert(std::is_same_v<Selection_aper::storage_ordinal_1_wrapper, Selection_count>);
static_assert(std::is_same_v<decltype(std::declval<Selection_count>().value), Selection_aper::storage_ordinal_1_payload_type>);
static_assert(std::is_same_v<Selection_aper::storage_ordinal_1_payload_type, Count>);
static_assert(std::is_same_v<Selection_aper::storage_ordinal_1_payload_mapping, Count_aper>);
static_assert(Packet_aper::field_count == 3 && Packet_aper::optional_bitmap_bit_count == 1);
static_assert(Packet_aper::field_count_declaration_ordinal == 0);
static_assert(Packet_aper::field_count_mandatory);
static_assert(Packet_aper::field_count_optional_bitmap_ordinal == static_cast<std::size_t>(-1));
static_assert(Packet_aper::field_enabled_declaration_ordinal == 1);
static_assert(!Packet_aper::field_enabled_mandatory);
static_assert(Packet_aper::field_enabled_optional_bitmap_ordinal == 0);
static_assert(Packet_aper::field_selection_declaration_ordinal == 2);
static_assert(Packet_aper::field_selection_mandatory);
static_assert(Packet_aper::field_selection_optional_bitmap_ordinal == static_cast<std::size_t>(-1));
static_assert(std::is_same_v<Packet_aper::field_count_payload_mapping, Count_aper>);
static_assert(std::is_same_v<Packet_aper::field_enabled_payload_mapping, APER_BOOLEAN>);
static_assert(std::is_same_v<Packet_aper::field_selection_payload_mapping, Selection_aper>);
static_assert(!Packet_aper::has_extension_bit && !Selection_aper::has_extension_bit);
static_assert(APER_COMPLETE_ENCODING::outermost_only_final_zero_padding);
static_assert(std::variant_size_v<nrforge::synthetic::reversed::Selection> == 2);
static_assert(nrforge::synthetic::reversed::Selection_aper::storage_alternative_to_per_root_index[0] == 1);
static_assert(nrforge::synthetic::reversed::Selection_aper::storage_alternative_to_per_root_index[1] == 0);
static_assert(nrforge::synthetic::reversed::Selection_aper::per_root_index_to_storage_ordinal[0] == 1);
static_assert(nrforge::synthetic::reversed::Selection_aper::per_root_index_to_storage_ordinal[1] == 0);
static_assert(nrforge::synthetic::reversed::Selection_aper::storage_alternative_to_per_root_index[nrforge::synthetic::reversed::Selection_aper::per_root_index_to_storage_ordinal[0]] == 0);
static_assert(nrforge::synthetic::reversed::Selection_aper::storage_alternative_to_per_root_index[nrforge::synthetic::reversed::Selection_aper::per_root_index_to_storage_ordinal[1]] == 1);
static_assert(std::is_same_v<nrforge::synthetic::reversed::Selection_aper::storage_ordinal_0_wrapper, std::variant_alternative_t<0, nrforge::synthetic::reversed::Selection>>);
static_assert(std::is_same_v<nrforge::synthetic::reversed::Selection_aper::storage_ordinal_0_wrapper, nrforge::synthetic::reversed::Selection_count>);
static_assert(std::is_same_v<decltype(std::declval<nrforge::synthetic::reversed::Selection_count>().value), nrforge::synthetic::reversed::Selection_aper::storage_ordinal_0_payload_type>);
static_assert(std::is_same_v<nrforge::synthetic::reversed::Selection_aper::storage_ordinal_0_payload_type, nrforge::synthetic::reversed::Count>);
static_assert(std::is_same_v<nrforge::synthetic::reversed::Selection_aper::storage_ordinal_0_payload_mapping, nrforge::synthetic::reversed::Count_aper>);
static_assert(std::is_same_v<nrforge::synthetic::reversed::Selection_aper::storage_ordinal_1_wrapper, std::variant_alternative_t<1, nrforge::synthetic::reversed::Selection>>);
static_assert(std::is_same_v<nrforge::synthetic::reversed::Selection_aper::storage_ordinal_1_wrapper, nrforge::synthetic::reversed::Selection_flag>);
static_assert(std::is_same_v<decltype(std::declval<nrforge::synthetic::reversed::Selection_flag>().value), nrforge::synthetic::reversed::Selection_aper::storage_ordinal_1_payload_type>);
static_assert(std::is_same_v<nrforge::synthetic::reversed::Selection_aper::storage_ordinal_1_payload_type, bool>);
static_assert(std::is_same_v<nrforge::synthetic::reversed::Selection_aper::storage_ordinal_1_payload_mapping, nrforge::synthetic::reversed::APER_BOOLEAN>);
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
