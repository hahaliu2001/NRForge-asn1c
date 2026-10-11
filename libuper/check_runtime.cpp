#include "runtime.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace nrforge::uper;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "UPER:%d: %s\n", __LINE__, #c);                                   \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
// Override new only for reproducible focused allocation failure windows.
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
static Reader reader(const CompleteEncoding &e, Limits l = {}) {
    auto r = Reader::make(e.octets, l);
    CHECK(r);
    return std::move(r).value();
}
static bool equal(std::span<const std::byte> a, std::span<const std::byte> b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}
static void ranges() {
    const std::uint64_t max = ~std::uint64_t{0};
    for (unsigned offset = 0; offset < 8; ++offset)
        for (auto upper : {0ULL, 1ULL, 2ULL, 254ULL, 255ULL, 256ULL, 65535ULL, 65536ULL,
                           549755813887ULL, 18446744073709551615ULL})
            for (std::uint64_t value : {std::uint64_t{0}, static_cast<std::uint64_t>(upper / 2),
                                        static_cast<std::uint64_t>(upper)}) {
                Writer w;
                CHECK(w.write_bits(0, offset));
                CHECK(w.write_uint(value, 0, upper));
                CHECK(w.cursor_bit() == offset + static_cast<unsigned>(std::bit_width(upper)));
                auto e = w.finish();
                CHECK(e);
                auto r = reader(e.value());
                CHECK(r.read_bits(offset));
                auto v = r.read_uint(0, upper);
                CHECK(v && v.value() == value);
                CHECK(r.finish());
            }
    for (auto v : {std::numeric_limits<std::int64_t>::min(), std::int64_t{-129}, std::int64_t{-128},
                   std::int64_t{-1}, std::int64_t{0}, std::int64_t{127}, std::int64_t{128},
                   std::numeric_limits<std::int64_t>::max()}) {
        Writer w;
        CHECK(w.write_int(v, std::numeric_limits<std::int64_t>::min(),
                          std::numeric_limits<std::int64_t>::max()));
        CHECK(w.write_unconstrained_int(v));
        CHECK(w.write_extensible_int(v, -4, 4));
        auto e = w.finish();
        CHECK(e);
        auto r = reader(e.value());
        CHECK(r.read_int(std::numeric_limits<std::int64_t>::min(),
                         std::numeric_limits<std::int64_t>::max())
                  .value() == v);
        CHECK(r.read_unconstrained_int().value() == v);
        CHECK(r.read_extensible_int(-4, 4).value() == v);
        CHECK(r.finish());
    }
    for (auto v :
         {std::uint64_t{0}, std::uint64_t{128}, std::uint64_t{255}, std::uint64_t{1} << 63, max}) {
        Writer w;
        CHECK(w.write_extensible_uint(v, 1, 3));
        CHECK(w.write_normally_small(v));
        CHECK(w.write_index({true, v}, 3, true));
        auto e = w.finish();
        CHECK(e);
        auto r = reader(e.value());
        CHECK(r.read_extensible_uint(1, 3).value() == v);
        CHECK(r.read_normally_small().value() == v);
        auto idx = r.read_index(3, true);
        CHECK(idx && idx.value().extension && idx.value().index == v);
        CHECK(r.finish());
    }
}
static void strings() {
    for (unsigned offset = 0; offset < 8; ++offset)
        for (std::size_t n : {0u, 1u, 2u, 16u, 127u, 128u, 16383u, 16384u, 16385u, 65535u, 65536u,
                              65537u, 131072u}) {
            std::vector<std::byte> v(n, std::byte{0xa5});
            Writer w;
            CHECK(w.write_bits(0, offset));
            CHECK(w.write_octets(v, {0, 0, true}));
            auto e = w.finish();
            CHECK(e);
            auto r = reader(e.value());
            CHECK(r.read_bits(offset));
            auto got = r.read_octets({0, 0, true});
            CHECK(got && got.value() == v);
            CHECK(r.finish());
            BitString b{std::vector<std::byte>(detail::octets_for_bits(n), std::byte{0xa5}), n};
            if (n % 8)
                b.octets.back() &= std::byte{static_cast<unsigned char>(0xffu << (8 - n % 8))};
            Writer bw;
            CHECK(bw.write_bits(0, offset));
            CHECK(bw.write_bit_string(b, {0, 131072}));
            auto be = bw.finish();
            CHECK(be);
            auto br = reader(be.value());
            CHECK(br.read_bits(offset));
            auto bg = br.read_bit_string({0, 131072});
            CHECK(bg && bg.value().bit_count == n && bg.value().octets == b.octets);
            CHECK(br.finish());
        }
    for (std::size_t n : {0u, 1u, 3u, 4u, 9u}) {
        std::vector<std::byte> v(n, std::byte{0x81});
        Writer w;
        CHECK(w.write_octets(v, {1, 3, false, true}));
        auto e = w.finish();
        CHECK(e);
        auto r = reader(e.value());
        CHECK(r.read_octets({1, 3, false, true}).value() == v);
        CHECK(r.finish());
    }
    for (std::size_t n : {1u, 63u, 64u, 65u, 127u, 128u, 1024u}) {
        BitString b{std::vector<std::byte>(detail::octets_for_bits(n), std::byte{0}), n};
        b.octets[0] = std::byte{0x80};
        Writer w;
        CHECK(w.write_extension_bitmap(b));
        auto e = w.finish();
        CHECK(e);
        auto r = reader(e.value());
        auto got = r.read_extension_bitmap();
        CHECK(got && got.value().bit_count == n && got.value().octets == b.octets);
        CHECK(r.finish());
    }
}
static void failure() {
    std::byte unused[] = {std::byte{0xe0}};
    auto made = Reader::make(unused, {}, 3);
    CHECK(made);
    auto r = std::move(made).value();
    auto bad = r.read_uint(0, 4);
    CHECK(!bad && bad.error().code == ErrorCode::constraint_violation && r.cursor_bit() == 0);
    CHECK(!r.read_bits(1) && r.first_error()->code == ErrorCode::constraint_violation);
    Writer w;
    CHECK(w.write_bits(5, 3));
    auto before = std::vector<std::byte>(w.staged_bytes().begin(), w.staged_bytes().end());
    CHECK(!w.write_uint(5, 0, 4));
    CHECK(w.cursor_bit() == 3 && equal(w.staged_bytes(), before));
    CHECK(!w.finish());
    for (auto raw :
         {std::vector<std::byte>{std::byte{0xc0}}, std::vector<std::byte>{std::byte{0xc5}},
          std::vector<std::byte>{std::byte{0x80}, std::byte{0x01}},
          std::vector<std::byte>{std::byte{0x81}}}) {
        auto mr = Reader::make(raw);
        CHECK(mr);
        auto rr = std::move(mr).value();
        CHECK(!rr.read_octets({0, 0, true}) && rr.cursor_bit() == 0 &&
              rr.usage().value_octets == 0);
    }
    // Every bit truncation of a compound primitive encoding rolls back the field.
    Writer sw;
    std::vector<std::byte> data(129, std::byte{0xa5});
    CHECK(sw.write_bits(1, 3));
    CHECK(sw.write_octets(data, {0, 0, true}));
    auto se = sw.finish();
    CHECK(se);
    for (std::size_t bits = 3; bits < se.value().semantic_bits; ++bits) {
        auto mr = Reader::make(se.value().octets, {}, bits);
        CHECK(mr);
        auto rr = std::move(mr).value();
        CHECK(rr.read_bits(3));
        CHECK(!rr.read_octets({0, 0, true}) && rr.cursor_bit() == 3 &&
              rr.usage().value_octets == 0);
    }
    // A fragment must have a subsequent determinant, even after an exact multiple.
    Writer fw;
    CHECK(fw.write_octets(std::vector<std::byte>(16384), {0, 0, true}));
    auto fe = fw.finish();
    CHECK(fe);
    fe.value().octets.pop_back();
    auto fr = Reader::make(fe.value().octets);
    CHECK(fr);
    CHECK(!fr.value().read_octets({0, 0, true}) && fr.value().cursor_bit() == 0);
    std::byte redundant[] = {std::byte{2}, std::byte{0}, std::byte{1}};
    auto ir = Reader::make(redundant);
    CHECK(ir);
    CHECK(!ir.value().read_unconstrained_int() && ir.value().cursor_bit() == 0);
    Writer bs;
    BitString invalid{{std::byte{0x81}}, 1};
    CHECK(!bs.write_bit_string(invalid, {1, 1}) && bs.cursor_bit() == 0);
    Limits l;
    l.max_output_octets = 1;
    Writer out(l);
    CHECK(out.write_bits(3, 2));
    auto prev = std::vector<std::byte>(out.staged_bytes().begin(), out.staged_bytes().end());
    CHECK(!out.write_octets(std::vector<std::byte>(20), {20, 20}));
    CHECK(out.cursor_bit() == 2 && equal(out.staged_bytes(), prev) &&
          out.usage().value_octets == 0);
    l = {};
    l.max_value_octets = 1;
    auto limited = Reader::make(se.value().octets, l);
    CHECK(limited);
    CHECK(limited.value().read_bits(3));
    CHECK(!limited.value().read_octets({0, 0, true}) && limited.value().cursor_bit() == 3 &&
          limited.value().usage().value_octets == 0);
    l = {};
    l.max_collection_elements = 2;
    Writer coll(l);
    CHECK(coll.write_collection_length(2, 0, 4));
    CHECK(!coll.write_collection_length(1, 0, 4) && coll.usage().collection_elements == 2);
    l = {};
    l.max_bitmap_bits = 1;
    Writer bm(l);
    CHECK(!bm.write_extension_bitmap({{std::byte{0}}, 2}) && bm.usage().bitmap_bits == 0);
    l = {};
    l.max_wire_bits = 7;
    Writer wire(l);
    CHECK(wire.write_bits(0, 7));
    CHECK(!wire.finish() && wire.cursor_bit() == 7);
    l = {};
    l.max_input_octets = 0;
    CHECK(!Reader::make(unused, l));
}
static void complete_and_open() {
    Writer empty;
    auto e = empty.finish();
    CHECK(e && e.value().octets == std::vector<std::byte>{std::byte{0}} &&
          e.value().semantic_bits == 0 && e.value().encoded_bits == 8 &&
          e.value().empty_substitution);
    auto r = reader(e.value());
    CHECK(r.finish());
    CHECK(!r.finish());
    CHECK(!empty.write_bits(0, 0));
    Writer contained;
    auto ce = contained.finish(Completion::contained_bits);
    CHECK(ce && ce.value().encoded_bits == 1);
    auto cr = Reader::make(ce.value().octets, {}, 1);
    CHECK(cr && cr.value().finish(Completion::contained_bits));
    Writer exact;
    auto ex = exact.finish(Completion::exact);
    CHECK(ex && ex.value().octets.empty() && ex.value().encoded_bits == 0);
    auto er = Reader::make(ex.value().octets, {}, 0);
    CHECK(er && er.value().finish(Completion::exact));
    Writer one;
    CHECK(one.write_bits(1, 1));
    auto oe = one.finish();
    CHECK(oe);
    oe.value().octets[0] |= std::byte{1};
    auto pr = reader(oe.value());
    CHECK(pr.read_bits(1));
    CHECK(!pr.finish() && pr.first_error()->code == ErrorCode::nonzero_padding &&
          pr.cursor_bit() == 1);
    std::byte trailing[] = {std::byte{0}, std::byte{0}};
    auto tr = Reader::make(trailing);
    CHECK(tr && !tr.value().finish());
    Writer open;
    std::vector<std::byte> payload(65536, std::byte{0xa5});
    CHECK(open.write_bits(1, 1));
    CHECK(open.write_open_type(payload));
    auto o = open.finish();
    CHECK(o);
    auto rr = reader(o.value());
    CHECK(rr.read_bits(1));
    auto got = rr.read_open_type(true);
    CHECK(got && got.value() == payload && rr.usage().unknown_records == 1 &&
          rr.usage().unknown_octets == payload.size());
    CHECK(rr.finish());
    auto sk = reader(o.value());
    CHECK(sk.read_bits(1));
    CHECK(sk.skip_open_type() && sk.usage().unknown_records == 0 && sk.usage().value_octets == 0);
    CHECK(sk.finish());
    Limits limit;
    limit.max_unknown_records = 0;
    auto nr = reader(o.value(), limit);
    CHECK(nr.read_bits(1));
    CHECK(!nr.read_open_type(true) && nr.cursor_bit() == 1 && !nr.usage().value_octets &&
          !nr.usage().unknown_octets);
    std::byte zero[] = {std::byte{0}};
    auto zr = Reader::make(zero);
    CHECK(zr && !zr.value().read_open_type());
    Writer no;
    CHECK(!no.write_open_type({}));
}
static void extra_limits() {
    Writer w;
    std::byte payload[] = {std::byte{0}};
    CHECK(w.write_open_type(payload));
    auto encoding = w.finish();
    CHECK(encoding);
    for (bool records : {false, true}) {
        Limits limits;
        if (records)
            limits.max_unknown_records = 0;
        else
            limits.max_unknown_octets = 0;
        auto r = reader(encoding.value(), limits);
        armed = true;
        allocation = 0;
        fail_at = 0;
        auto status = r.read_open_type(true);
        armed = false;
        CHECK(!status && status.error().code == ErrorCode::resource_limit && allocation == 0 &&
              r.cursor_bit() == 0 && !r.usage().unknown_records && !r.usage().unknown_octets &&
              !r.usage().value_octets);
    }
    fail_at = ~std::size_t{0};
    for (auto raw : {std::vector<std::byte>{std::byte{0}}, std::vector<std::byte>{std::byte{0xc0}},
                     std::vector<std::byte>{std::byte{0xc5}},
                     std::vector<std::byte>{std::byte{0x80}, std::byte{1}},
                     std::vector<std::byte>{std::byte{0xc1}}}) {
        auto made = Reader::make(raw);
        CHECK(made);
        auto r = std::move(made).value();
        CHECK(!r.skip_open_type() && r.cursor_bit() == 0 && !r.usage().value_octets);
    }
    std::byte invalid_index[] = {std::byte{0x60}};
    auto idx = Reader::make(invalid_index);
    CHECK(idx && !idx.value().read_index(3, true) && idx.value().cursor_bit() == 0);
    Writer coll;
    CHECK(coll.write_collection_segment(1, false));
    CHECK(coll.write_collection_segment(1, false));
    auto ce = coll.finish();
    CHECK(ce);
    Limits limits;
    limits.max_collection_elements = 1;
    auto r = reader(ce.value(), limits);
    CHECK(r.read_collection_segment());
    CHECK(!r.read_collection_segment() && r.cursor_bit() == 8 &&
          r.usage().collection_elements == 1);
    auto moved = reader(encoding.value());
    auto target = std::move(moved);
    CHECK(!moved.read_bits(1));
    CHECK(target.read_open_type());
    CHECK(target.finish());
}
static void aliases() {
    for (bool open : {false, true})
        for (bool sub : {false, true}) {
            Writer w;
            for (unsigned i = 0; i < 128; ++i)
                CHECK(w.write_bits(0xaa, 8));
            auto before = std::vector<std::byte>(w.staged_bytes().begin(), w.staged_bytes().end());
            auto span = w.staged_bytes();
            if (sub)
                span = span.subspan(1);
            auto r = open ? w.write_open_type(span) : w.write_octets(span, {0, 0, true});
            CHECK(!r && r.error().code == ErrorCode::invalid_argument && w.cursor_bit() == 1024 &&
                  equal(w.staged_bytes(), before) && !w.usage().value_octets);
        }
    Writer w;
    CHECK(!w.finish(static_cast<Completion>(99)) && w.cursor_bit() == 0);
    std::byte z[] = {std::byte{0}};
    auto r = Reader::make(z);
    CHECK(r && !r.value().finish(static_cast<Completion>(99)) && r.value().cursor_bit() == 0);
}
static void oom() {
    std::vector<std::byte> value(65537, std::byte{0xa5});
    Writer baseline;
    CHECK(baseline.write_bits(5, 3));
    armed = true;
    allocation = 0;
    CHECK(baseline.write_octets(value, {0, 0, true}));
    armed = false;
    auto count = allocation;
    CHECK(count > 0);
    auto encoding = baseline.finish();
    CHECK(encoding);
    for (fail_at = 0; fail_at < count; ++fail_at) {
        Writer w;
        CHECK(w.write_bits(5, 3));
        auto before = std::vector<std::byte>(w.staged_bytes().begin(), w.staged_bytes().end());
        armed = true;
        allocation = 0;
        auto status = w.write_octets(value, {0, 0, true});
        armed = false;
        CHECK(!status && status.error().code == ErrorCode::allocation_failure &&
              w.cursor_bit() == 3 && equal(w.staged_bytes(), before) &&
              w.usage().value_octets == 0);
    }
    fail_at = ~std::size_t{0};
    auto base = reader(encoding.value());
    CHECK(base.read_bits(3));
    armed = true;
    allocation = 0;
    auto value_read = base.read_open_type(true);
    armed = false;
    CHECK(value_read);
    auto reads = allocation;
    CHECK(reads > 0);
    for (fail_at = 0; fail_at < reads; ++fail_at) {
        auto r = reader(encoding.value());
        CHECK(r.read_bits(3));
        armed = true;
        allocation = 0;
        auto status = r.read_open_type(true);
        armed = false;
        CHECK(!status && status.error().code == ErrorCode::allocation_failure &&
              r.cursor_bit() == 3 && !r.usage().value_octets && !r.usage().unknown_records);
    }
    fail_at = ~std::size_t{0};
    std::printf("allocation rollback encode=%zu decode=%zu PASS\n", count, reads);
}
int main() {
    ranges();
    strings();
    failure();
    complete_and_open();
    aliases();
    extra_limits();
    oom();
    std::puts("UPER primitive boundaries, budgets, padding, framing and rollback PASS");
}
