#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/asn1typed-s4-variants.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
render() {
    mkdir -p "$work/$1"
    ./check_asn1typed_codec_render "fixtures/$2.asn1" "$3" "$4" "$work/$1"
}
render renamed s4-renamed-num S4RenamedNum nrforge::s4::renamed
render reordered s4-reordered S4Reordered nrforge::s4::reordered
render extra s4-extra-boolean S4ExtraBoolean nrforge::s4::extra
render swap s4-type-swap S4TypeSwap nrforge::s4::swap
render choice s4-choice-swap S4ChoiceSwap nrforge::s4::choice
render presence s4-presence S4Presence nrforge::s4::presence
render boolean s4-count-boolean S4CountBoolean nrforge::s4::boolean
render empty s4-empty-sequence S4EmptySequence nrforge::s4::empty
render two s4-two-renamed-types S4TwoRenamedTypes nrforge::s4::two
render nested1 cpp-aper-s1 CppAperSlice foo::nrforge
render nested2 cpp-aper-s1 CppAperSlice x::nrforge::y
cat > "$work/variants.cpp" <<'CPP'
#include <runtime.hpp>
#include "renamed/types.hpp"
#include "renamed/mapping.hpp"
#include "renamed/codec.hpp"
#include "reordered/types.hpp"
#include "reordered/mapping.hpp"
#include "reordered/codec.hpp"
#include "extra/types.hpp"
#include "extra/mapping.hpp"
#include "extra/codec.hpp"
#include "swap/types.hpp"
#include "swap/mapping.hpp"
#include "swap/codec.hpp"
#include "choice/types.hpp"
#include "choice/mapping.hpp"
#include "choice/codec.hpp"
#include "presence/types.hpp"
#include "presence/mapping.hpp"
#include "presence/codec.hpp"
#include "boolean/types.hpp"
#include "boolean/mapping.hpp"
#include "boolean/codec.hpp"
#include "empty/types.hpp"
#include "empty/mapping.hpp"
#include "empty/codec.hpp"
#include "two/types.hpp"
#include "two/mapping.hpp"
#include "two/codec.hpp"
#include "nested1/types.hpp"
#include "nested1/mapping.hpp"
#include "nested1/codec.hpp"
#include "nested2/types.hpp"
#include "nested2/mapping.hpp"
#include "nested2/codec.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#ifdef NDEBUG
#error "S4 variant checks require active assertions"
#endif
#include <cstdio>
#include <initializer_list>
#include <type_traits>
#include <vector>

static void expect(const nrforge::aper::Result<nrforge::aper::CompleteEncoding>& e,
                   std::initializer_list<unsigned> expected) {
    static unsigned vector_number = 0;
    ++vector_number;
    assert(e);
    if(e.value().octets.size() != expected.size())
        std::fprintf(stderr, "vector %u: got %zu octets, expected %zu\n", vector_number, e.value().octets.size(), expected.size());
    assert(e.value().octets.size() == expected.size());
    size_t i = 0;
    for(unsigned v : expected) {
        if(e.value().octets[i] != std::byte{static_cast<unsigned char>(v)})
            std::fprintf(stderr, "vector %u byte %zu: got %02x expected %02x\n", vector_number, i,
                         std::to_integer<unsigned>(e.value().octets[i]), v);
        assert(e.value().octets[i++] == std::byte{static_cast<unsigned char>(v)});
    }
}

int main() {
    {
        namespace n = nrforge::s4::renamed;
        n::Packet p{}; p.count = 1; p.enabled = true;
        p.selection = n::Selection_amount{n::Num{0x1234}};
        auto e = n::encode_packet(p); expect(e, {0x80,0x00,0x01,0xc0,0x12,0x34});
        auto d = n::decode_packet(e.value().octets);
        assert(d && d.value().count == 1 && d.value().enabled == true);
        assert(std::get<n::Selection_amount>(d.value().selection).value == 0x1234);
    }
    {
        namespace n = nrforge::s4::reordered;
        n::Packet p{}; p.enabled.reset(); p.selection = n::Selection_flag{true}; p.count = 1;
        auto e = n::encode_packet(p); expect(e, {0x20,0x00,0x01});
        auto d = n::decode_packet(e.value().octets);
        assert(d && !d.value().enabled && d.value().count == 1);
        assert(std::get<n::Selection_flag>(d.value().selection).value);
        p.enabled = false;
        auto present = n::encode_packet(p); expect(present, {0x90,0x00,0x01});
        auto dp = n::decode_packet(present.value().octets);
        assert(dp && dp.value().enabled == false && dp.value().count == 1);
        assert(std::get<n::Selection_flag>(dp.value().selection).value);
    }
    {
        namespace n = nrforge::s4::extra;
        n::Packet p{}; p.count = 1; p.enabled.reset();
        p.selection = n::Selection_flag{true}; p.extra = true;
        auto e = n::encode_packet(p); expect(e, {0x00,0x00,0x01,0x60});
        auto d = n::decode_packet(e.value().octets);
        assert(d && d.value().count == 1 && !d.value().enabled && d.value().extra);
        assert(std::get<n::Selection_flag>(d.value().selection).value);
    }
    {
        namespace n = nrforge::s4::swap;
        n::Packet p{}; p.count = true; p.enabled = n::Count{0x1234};
        p.selection = n::Selection_amount{n::Count{0x00ff}};
        auto e = n::encode_packet(p); expect(e, {0xc0,0x12,0x34,0x80,0x00,0xff});
        auto d = n::decode_packet(e.value().octets);
        assert(d && d.value().count && d.value().enabled == n::Count{0x1234});
        assert(std::get<n::Selection_amount>(d.value().selection).value == 0x00ff);
        p.enabled.reset(); p.selection = n::Selection_flag{false};
        auto absent = n::encode_packet(p); expect(absent, {0x40});
        auto da = n::decode_packet(absent.value().octets);
        assert(da && da.value().count && !da.value().enabled);
        assert(!std::get<n::Selection_flag>(da.value().selection).value);
    }
    {
        namespace n = nrforge::s4::choice;
        n::Selection integer = n::Selection_amount{n::Num{0x1234}};
        auto e = n::encode_selection(integer); expect(e, {0x80,0x12,0x34});
        auto d = n::decode_selection(e.value().octets);
        assert(d && std::get<n::Selection_amount>(d.value()).value == 0x1234);
        n::Selection boolean = n::Selection_switch_{true};
        auto b = n::encode_selection(boolean); expect(b, {0x40});
        auto db = n::decode_selection(b.value().octets);
        assert(db && std::get<n::Selection_switch_>(db.value()).value);
    }
    {
        namespace n = nrforge::s4::presence;
        n::Packet p{}; p.count = 1; p.enabled = true;
        p.selection = n::Selection_flag{true}; p.extra = true; p.trailing = false;
        auto e = n::encode_packet(p); expect(e, {0xc0,0x00,0x01,0xb0});
        auto d = n::decode_packet(e.value().octets);
        assert(d && d.value().count == 1 && d.value().enabled);
        assert(std::get<n::Selection_flag>(d.value().selection).value);
        assert(d.value().extra == true && d.value().trailing == false);
        p.extra.reset(); p.trailing = true;
        auto absent = n::encode_packet(p); expect(absent, {0x40,0x00,0x01,0xb0});
        auto da = n::decode_packet(absent.value().octets);
        assert(da && da.value().count == 1 && da.value().enabled == true);
        assert(std::get<n::Selection_flag>(da.value().selection).value);
        assert(!da.value().extra && da.value().trailing == true);
    }
    {
        namespace n = nrforge::s4::boolean;
        auto e = n::encode_count(true); expect(e, {0x80});
        auto d = n::decode_count(e.value().octets); assert(d && d.value());
        n::Selection v = n::Selection_amount{true};
        auto c = n::encode_selection(v); expect(c, {0xc0});
        auto dc = n::decode_selection(c.value().octets);
        assert(dc && std::get<n::Selection_amount>(dc.value()).value);
    }
    {
        namespace n = nrforge::s4::empty;
        n::Empty value{};
        auto e = n::encode_empty(value);
        assert(e && e.value().octets.size() == 1 && e.value().octets[0] == std::byte{0});
        assert(e.value().last_field_end_bit == 0);
        assert(e.value().final_padding_bits == 0);
        assert(e.value().empty_encoding_substitution);
        assert(e.value().complete_encoding_bits == 8 && e.value().octet_count == 1);
        const std::array<std::byte,1> wire{std::byte{0}};
        auto d = n::decode_empty(wire); assert(d);
        const std::array<std::byte,0> empty{};
        auto de = n::decode_empty(empty);
        assert(!de && de.error().code == nrforge::aper::ErrorCode::truncated_input);
        assert(de.error().bit_offset == 0);
        const std::array<std::byte,1> nonzero{std::byte{0x80}};
        auto dn = n::decode_empty(nonzero);
        assert(!dn && dn.error().code == nrforge::aper::ErrorCode::nonzero_padding);
        assert(dn.error().bit_offset == 0);
        const std::array<std::byte,2> trailing{std::byte{0},std::byte{0}};
        auto dt = n::decode_empty(trailing);
        assert(!dt && dt.error().code == nrforge::aper::ErrorCode::trailing_data);
        assert(dt.error().bit_offset == 8);
    }
    {
        namespace n = nrforge::s4::two;
        static_assert(std::is_same_v<n::Number, std::uint64_t>);
        static_assert(std::is_same_v<decltype(n::Envelope{}.value), n::Number>);
        n::Envelope value{}; value.value = 0x1234; value.marker.reset();
        auto e = n::encode_envelope(value); expect(e, {0x00,0x12,0x34});
        auto d = n::decode_envelope(e.value().octets);
        assert(d && d.value().value == 0x1234 && !d.value().marker);
        value.marker = true;
        auto p = n::encode_envelope(value); expect(p, {0x80,0x12,0x34,0x80});
        auto dp = n::decode_envelope(p.value().octets);
        assert(dp && dp.value().value == 0x1234 && dp.value().marker == true);
    }
    {
        foo::nrforge::Packet p{}; p.count = 1; p.selection = foo::nrforge::Selection_flag{true};
        auto e = foo::nrforge::encode_packet(p); expect(e, {0x00,0x00,0x01,0x40});
        auto d = foo::nrforge::decode_packet(e.value().octets);
        assert(d && d.value().count == 1 && !d.value().enabled && std::get<foo::nrforge::Selection_flag>(d.value().selection).value);
        x::nrforge::y::Packet q{}; q.count = 1; q.selection = x::nrforge::y::Selection_flag{true};
        auto qe = x::nrforge::y::encode_packet(q); expect(qe, {0x00,0x00,0x01,0x40});
        auto qd = x::nrforge::y::decode_packet(qe.value().octets);
        assert(qd && qd.value().count == 1 && !qd.value().enabled && std::get<x::nrforge::y::Selection_flag>(qd.value().selection).value);
    }
}
CPP
${CXX:-c++} -UNDEBUG -std=c++20 -Wall -Wextra -Werror -pedantic -I../libaper -I"$work" "$work/variants.cpp" ../libaper/runtime.cpp -o "$work/variants"
"$work/variants"
echo 'PASS S4 Parser/Fixer/Owned IR structure and naming variants'
