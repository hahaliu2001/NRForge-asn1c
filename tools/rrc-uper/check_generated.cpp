#include "r000.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace nrforge::uper;
namespace g = nrforge_generated::r000;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "generated:%d: %s\n", __LINE__, #x);                              \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
static bool armed = false;
static std::size_t allocation = 0, fail_at = ~std::size_t{0};
void *operator new(std::size_t n) {
    if (armed && allocation++ == fail_at)
        throw std::bad_alloc();
    if (auto p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
g::T1 body() {
    g::T1 b;
    b.f_small.value = {{1}, {2}};
    b.f_box.f_value = {4};
    b.f_ext_list.value = {{3}};
    return b;
}
static CompleteEncoding roundtrip(const char *name, const g::Root &value) {
    auto e = g::encode(value);
    CHECK(e);
    auto d = g::decode(e.value().octets);
    CHECK(d && d.value().value == value && d.value().semantic_bits == e.value().semantic_bits);
    if (name) {
        std::printf("%s ", name);
        for (auto b : e.value().octets)
            std::printf("%02x", std::to_integer<unsigned>(b));
        std::printf(" %zu\n", e.value().semantic_bits);
    }
    return std::move(e).value();
}
int main() {
    static_assert(g::T3::wire0 == Index{false, 1});
    static_assert(g::T3::wire1 == Index{false, 0});
    static_assert(g::T5::number0 == 9 && g::T5::e0 == Index{false, 1});
    auto b = body();
    CHECK(b.f_counter.value == 3 && b.f_mode.value == Index({false, 1}));
    g::Root root;
    root.value = g::T0::A0{b};
    auto minimal = roundtrip("minimal", root);
    b.f_counter = {31};
    b.f_mode = {Index{false, 0}};
    b.f_flag = true;
    b.f_ext_list.value = {{1}, {2}, {3}, {4}};
    b.f_added = g::T4{9};
    g::T9 group;
    group.f_required = {11};
    group.f_maybe = false;
    group.f_fallback = {7};
    b.f_group_1 = group;
    root.value = g::T0::A0{b};
    auto extended = roundtrip("group", root);
    g::T3 pick;
    pick.value = g::T3::A1{};
    root.value = g::T0::A2{pick};
    roundtrip("choice_root", root);
    pick.value = g::T3::A2{g::T6{{{3}}}};
    root.value = g::T0::A2{pick};
    roundtrip("choice_extension", root);
    root.value = g::T0::A3{body()};
    roundtrip("root_extension", root);
    g::T2 large;
    large.value.resize(65536);
    root.value = g::T0::A1{large};
    auto fragmented = roundtrip(nullptr, root);
    CHECK(fragmented.semantic_bits == 19);
    root.value = g::Unknown{std::numeric_limits<std::uint64_t>::max(), {std::byte{0}}};
    auto unknown = roundtrip(nullptr, root);
    Limits budget;
    budget.max_unknown_records = 0;
    auto no_unknown = g::decode(unknown.octets, budget);
    CHECK(!no_unknown && no_unknown.error().code == ErrorCode::resource_limit);
    root.value = g::Unknown{0, {std::byte{0}}};
    CHECK(!g::encode(root));
    root.value = std::monostate{};
    CHECK(!g::encode(root));
    // Future SEQUENCE additions are framing-validated and discarded.
    Writer future;
    g::take(future.write_index({false, 0}, 3, true));
    g::take(future.write_bits(1, 1));
    g::take(future.write_bits(0, 3));
    auto plain = body();
    g::take(g::put6(future, plain.f_small));
    g::take(g::put7(future, plain.f_box));
    g::take(g::put8(future, plain.f_ext_list));
    g::take(future.write_extension_bitmap({{std::byte{0x20}}, 3}));
    std::byte zero[] = {std::byte{0}};
    g::take(future.write_open_type(zero));
    auto fe = g::take(future.finish());
    auto fd = g::decode(fe.octets);
    CHECK(fd && std::get<g::T0::A0>(fd.value().value.value).value == plain);
    for (std::size_t n = 0; n < extended.semantic_bits; ++n) {
        auto made = Reader::make(extended.octets, {}, n);
        CHECK(made);
        auto r = std::move(made).value();
        auto value = g::get0(r);
        CHECK(!value && r.cursor_bit() == 0 && !r.usage().collection_elements &&
              !r.usage().value_octets && !r.usage().work_bits);
    }
    auto extra = minimal.octets;
    extra.push_back(std::byte{0});
    CHECK(!g::decode(extra));
    for (unsigned which = 0; which < 4; ++which) {
        Limits l;
        if (which == 0)
            l.max_collection_elements = 1;
        else if (which == 1)
            l.max_depth = 1;
        else if (which == 2)
            l.max_work_bits = 3;
        else
            l.max_value_octets = 0;
        auto decoded = g::decode(minimal.octets, l);
        CHECK(!decoded && decoded.error().code == ErrorCode::resource_limit);
    }
    auto invalid = body();
    invalid.f_box.f_value.value = 32;
    root.value = g::T0::A0{invalid};
    Writer rollback;
    g::take(rollback.write_bits(5, 3));
    CHECK(!g::put0(rollback, root) && rollback.cursor_bit() == 3 &&
          rollback.usage().work_bits == 3 && !rollback.usage().collection_elements);
    root.value = g::T0::A0{b};
    auto encoded = g::encode(root);
    CHECK(encoded);
    Writer baseline;
    g::take(baseline.write_bits(5, 3));
    armed = true;
    allocation = 0;
    auto status = g::put0(baseline, root);
    armed = false;
    CHECK(status);
    auto enc_alloc = allocation;
    CHECK(enc_alloc);
    for (fail_at = 0; fail_at < enc_alloc; ++fail_at) {
        Writer w;
        g::take(w.write_bits(5, 3));
        armed = true;
        allocation = 0;
        auto status = g::put0(w, root);
        armed = false;
        CHECK(!status && status.error().code == ErrorCode::allocation_failure &&
              w.cursor_bit() == 3 && w.usage().work_bits == 3 && !w.usage().collection_elements &&
              !w.usage().open_staging_octets && !w.usage().value_octets);
    }
    fail_at = ~std::size_t{0};
    armed = true;
    allocation = 0;
    auto decoded = g::decode(encoded.value().octets);
    armed = false;
    CHECK(decoded);
    auto dec_alloc = allocation;
    CHECK(dec_alloc);
    for (fail_at = 0; fail_at < dec_alloc; ++fail_at) {
        auto made = Reader::make(encoded.value().octets);
        CHECK(made);
        auto r = std::move(made).value();
        armed = true;
        allocation = 0;
        auto status = g::get0(r);
        armed = false;
        CHECK(!status && status.error().code == ErrorCode::allocation_failure &&
              r.cursor_bit() == 0 && !r.usage().collection_elements && !r.usage().value_octets &&
              !r.usage().work_bits);
    }
    fail_at = ~std::size_t{0};
    std::printf("generated allocation rollback encode=%zu decode=%zu PASS\n", enc_alloc, dec_alloc);
    std::puts("generated UPER defaults/groups/choices/future policies/collections/rollback PASS");
}
