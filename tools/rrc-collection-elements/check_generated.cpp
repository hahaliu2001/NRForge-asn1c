#include "r000.hpp"
#include "r001.hpp"
#include "r002.hpp"
#include "r003.hpp"
#include "r004.hpp"
#include "r005.hpp"
#include "r006.hpp"
#include "r007.hpp"
#include "r008.hpp"
#include "r010.hpp"
#include <cstdio>
#include <cstdlib>

using namespace nrforge::uper;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "elements:%d: %s\n", __LINE__, #x); std::exit(1); } } while(false)

std::vector<std::byte> bytes(std::initializer_list<unsigned> values) {
    std::vector<std::byte> result;
    for (auto value : values) result.push_back(static_cast<std::byte>(value));
    return result;
}
template<class T, class Encode, class Decode>
void roundtrip(const T &v, Encode encode, Decode decode) {
    for (auto mode : {Completion::octets, Completion::contained_bits, Completion::exact}) {
        auto e = encode(v, {}, mode);
        CHECK(e);
        auto d = decode(e.value().octets, {}, mode == Completion::octets ? std::nullopt :
                        std::optional<std::size_t>{e.value().encoded_bits}, mode);
        CHECK(d && d.value().value == v && d.value().semantic_bits == e.value().semantic_bits);
    }
}
#define ROUND(N, V) roundtrip(V, nrforge_generated::N::encode, nrforge_generated::N::decode)

int main() {
    namespace i = nrforge_generated::r000;
    i::Root integers; integers.value = {{-2}, {3}};
    auto e = i::encode(integers);
    CHECK(e && e.value().semantic_bits == 8 && e.value().octets == bytes({0x45}));
    ROUND(r000, integers);
    for (auto bad : {-3, 4}) { integers.value[0].value = bad; CHECK(!i::encode(integers)); }
    integers.value.clear(); CHECK(!i::encode(integers));
    integers.value.assign(4, i::T1{0}); CHECK(!i::encode(integers));
    CHECK(!i::decode(bytes({0x38}))); // valid length, unused INTEGER index
    CHECK(!i::decode(bytes({0xc0}))); // unused collection length index
    CHECK(!i::decode(bytes({})));     // truncated determinant
    integers.value = {{-2}, {3}};
    Limits limit; limit.max_collection_elements = 1;
    auto encode_limit = i::encode(integers, limit);
    auto decode_limit = i::decode(e.value().octets, limit);
    CHECK(!encode_limit && encode_limit.error().code == ErrorCode::resource_limit);
    CHECK(!decode_limit && decode_limit.error().code == ErrorCode::resource_limit);
    limit = {}; limit.max_value_octets = 1;
    decode_limit = i::decode(e.value().octets, limit);
    CHECK(!decode_limit && decode_limit.error().code == ErrorCode::resource_limit);

    namespace s = nrforge_generated::r001;
    s::Root singles; singles.value = {{4}, {4}};
    auto es = s::encode(singles);
    CHECK(es && es.value().semantic_bits == 1 && es.value().octets == bytes({0x80}));
    ROUND(r001, singles);
    singles.value[1].value = 5; CHECK(!s::encode(singles));

    namespace b = nrforge_generated::r002;
    const BitString word{bytes({0x80, 0, 0, 1}), 32};
    b::Root bits; bits.value = {{word}};
    auto eb = b::encode(bits);
    CHECK(eb && eb.value().semantic_bits == 33 && eb.value().octets == bytes({0x40, 0, 0, 0, 0x80}));
    ROUND(r002, bits);
    bits.value[0].value.bit_count = 31; CHECK(!b::encode(bits));
    bits.value[0].value.bit_count = 33; CHECK(!b::encode(bits));
    bits.value[0].value = {bytes({0}), 32}; CHECK(!b::encode(bits));
    CHECK(!b::decode(bytes({0x40, 0, 0, 0}))); // truncated fixed-width element

    namespace q = nrforge_generated::r003;
    q::Root bounded; bounded.value = {{{bytes({0x80}), 1}}, {{bytes({0xa0}), 3}}};
    auto eq = q::encode(bounded);
    CHECK(eq && eq.value().semantic_bits == 10 && eq.value().octets == bytes({0x8d, 0x40}));
    ROUND(r003, bounded);
    q::Root empty; auto ee = q::encode(empty);
    CHECK(ee && ee.value().semantic_bits == 2 && ee.value().octets == bytes({0}));
    ROUND(r003, empty);
    bounded.value[0].value.bit_count = 0; CHECK(!q::encode(bounded));
    bounded.value[0].value.bit_count = 4; CHECK(!q::encode(bounded));
    CHECK(!q::decode(bytes({0x70}))); // list count 1, unused bit-length index 3

    namespace f = nrforge_generated::r004;
    f::Root fields; fields.f_first.value = {{7}}; fields.f_bits.value = {{word}};
    ROUND(r004, fields);
    fields.f_second = f::T2{}; fields.f_second->value = {{15}};
    ROUND(r004, fields);
    fields.f_second->value[0].value = 16; CHECK(!f::encode(fields));
    fields.f_second->value[0].value = 15;
    fields.f_first.value[0].value = 8; CHECK(!f::encode(fields));

    namespace c = nrforge_generated::r005;
    c::Root choice; choice.value = c::T0::A0{};
    std::get<c::T0::A0>(choice.value).value.value = {{139}};
    ROUND(r005, choice);
    std::get<c::T0::A0>(choice.value).value.value[0].value = 140; CHECK(!c::encode(choice));
    choice.value = c::T0::A1{};
    std::get<c::T0::A1>(choice.value).value.value = {{word}};
    ROUND(r005, choice);

    namespace n = nrforge_generated::r006;
    n::Root nested; nested.value.resize(2);
    nested.value[0].f_values.value = {{0}, {7}};
    ROUND(r006, nested);
    limit = {}; limit.max_collection_elements = 3; // outer 2 plus inner 2
    CHECK(!n::encode(nested, limit));
    auto en = n::encode(nested); CHECK(en && !n::decode(en.value().octets, limit));
    nested.value[0].f_values.value[1].value = 8; CHECK(!n::encode(nested));

    namespace g = nrforge_generated::r007;
    g::Root grouped; grouped.f_marker = true;
    ROUND(r007, grouped);
    grouped.f_group_0 = g::T1{}; grouped.f_group_0->f_values.value = {{7}};
    grouped.f_extra = g::T2{}; grouped.f_extra->value = {{word}};
    ROUND(r007, grouped);
    grouped.f_group_0->f_values.value[0].value = 8; CHECK(!g::encode(grouped));

    namespace w = nrforge_generated::r008;
    w::Root wide; wide.value = {{UINT64_MAX - 1}, {UINT64_MAX}};
    auto ew = w::encode(wide);
    CHECK(ew && ew.value().semantic_bits == 3 && ew.value().octets == bytes({0xa0}));
    ROUND(r008, wide);
    wide.value[0].value = UINT64_MAX - 2; CHECK(!w::encode(wide));

    namespace x = nrforge_generated::r010;
    x::Root extensible; extensible.value = {{3}};
    auto ex = x::encode(extensible);
    CHECK(ex && ex.value().semantic_bits == 4 && ex.value().octets == bytes({0x30}));
    ROUND(r010, extensible);
    extensible.value = {{7}, {-1}}; ROUND(r010, extensible);
    std::puts("PASS constrained collection codecs: 10 roots, 3 completion modes, determinant bytes/bits, bounds, truncation and quotas");
}
