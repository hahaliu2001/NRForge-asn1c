#include "runtime.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
using namespace nrforge::uper;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "compound:%d: %s\n", __LINE__, #x);                               \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
static bool armed = false;
static std::size_t allocations = 0, fail_at = ~std::size_t{0};
void *operator new(std::size_t n) {
    if (armed && allocations++ == fail_at)
        throw std::bad_alloc();
    if (auto p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
template <class T> T take(Result<T> r) {
    if (!r)
        throw detail::Fault{r.error()};
    return std::move(r).value();
}
void take(Result<void> r) {
    if (!r)
        throw detail::Fault{r.error()};
}
static void rollback() {
    Writer ex;
    take(ex.write_bits(5, 3));
    try {
        (void)ex.compound([](Writer &io) -> Result<void> {
            take(io.write_bits(0, 17));
            throw std::runtime_error("callback");
        });
        CHECK(false);
    } catch (const std::runtime_error &) {
    }
    CHECK(ex.cursor_bit() == 3 && ex.usage().work_bits == 3 &&
          ex.staged_bytes()[0] == std::byte{0xa0});
    take(ex.write_bits(0, 1));
    std::byte exdata[] = {std::byte{0xa0}};
    auto er = take(Reader::make(exdata));
    take(er.read_bits(3));
    try {
        (void)er.compound<unsigned>([](Reader &io) -> Result<unsigned> {
            take(io.read_bits(2));
            throw std::runtime_error("callback");
        });
        CHECK(false);
    } catch (const std::runtime_error &) {
    }
    CHECK(er.cursor_bit() == 3 && er.usage().work_bits == 3);
    take(er.read_bits(1));
    Writer w;
    take(w.write_bits(5, 3));
    auto original = std::vector<std::byte>(w.staged_bytes().begin(), w.staged_bytes().end());
    auto s = w.compound([&](Writer &io) {
        take(io.write_bits(7, 3));
        return io.compound([&](Writer &inner) {
            take(inner.write_bits(0, 9));
            return inner.write_uint(8, 0, 7);
        });
    });
    CHECK(!s && w.cursor_bit() == 3 && w.usage().work_bits == 3 &&
          std::equal(original.begin(), original.end(), w.staged_bytes().begin()) && !w.finish());
    Writer ignored;
    take(ignored.write_bits(5, 3));
    auto bad = ignored.compound([](Writer &io) {
        take(io.write_bits(0, 2));
        (void)io.write_uint(8, 0, 7);
        return Result<void>::success();
    });
    CHECK(!bad && ignored.cursor_bit() == 3 && ignored.usage().work_bits == 3);
    std::byte raw[] = {std::byte{0xa7}};
    auto r = take(Reader::make(raw));
    take(r.read_bits(3));
    auto no = r.compound<unsigned>([](Reader &io) {
        take(io.read_bits(2));
        (void)io.read_uint(0, 4);
        return Result<unsigned>::success(0);
    });
    CHECK(!no && r.cursor_bit() == 3 && r.usage().work_bits == 3);
    for (bool sub : {false, true}) {
        Writer alias;
        for (unsigned i = 0; i < 128; ++i)
            take(alias.write_bits(0xaa, 8));
        auto span = alias.staged_bytes();
        if (sub)
            span = span.subspan(1);
        auto before = std::vector<std::byte>(span.begin(), span.end());
        auto status = alias.write_extension_bitmap(span, span.size() * 8);
        CHECK(
            !status && status.error().code == ErrorCode::invalid_argument &&
            alias.cursor_bit() == 1024 &&
            std::equal(before.begin(), before.end(), alias.staged_bytes().begin() + (sub ? 1 : 0)));
    }
}
static Result<void> nested(Writer &w) {
    return w.write_known_open([](Writer &outer) {
        return outer.write_known_open([](Writer &) { return Result<void>::success(); });
    });
}
static void opens() {
    Writer w;
    take(w.write_bits(5, 3));
    take(nested(w));
    auto e = take(w.finish());
    CHECK(w.usage().open_staging_octets == 0 && w.usage().work_bits == 56);
    auto r = take(Reader::make(e.octets));
    take(r.read_bits(3));
    auto got = r.read_known_open<unsigned>([](Reader &outer) {
        return outer.read_known_open<unsigned>(
            [](Reader &) { return Result<unsigned>::success(99); });
    });
    CHECK(got && got.value() == 99 && r.usage().work_bits == 51);
    take(r.finish());
    CHECK(r.usage().work_bits == 56);
    for (unsigned limit_kind = 0; limit_kind < 3; ++limit_kind) {
        Limits l;
        if (limit_kind == 0)
            l.max_depth = 1;
        else if (limit_kind == 1)
            l.max_open_staging_octets = 2;
        else
            l.max_work_bits = 55;
        Writer limited(l);
        take(limited.write_bits(5, 3));
        auto status = nested(limited);
        if (limit_kind == 2 && status)
            status = limited.finish() ? Result<void>::success()
                                      : Result<void>::failure(*limited.first_error());
        CHECK(!status && status.error().code == ErrorCode::resource_limit);
        CHECK(limited.usage().open_staging_octets == 0);
        if (limit_kind != 2)
            CHECK(limited.cursor_bit() == 3 && limited.usage().work_bits == 3);
    }
    Limits l;
    l.max_depth = 0;
    Writer depth(l);
    CHECK(!depth.compound([](Writer &) { return Result<void>::success(); }));
    l = {};
    l.max_work_bits = 50;
    auto limited = take(Reader::make(e.octets, l));
    take(limited.read_bits(3));
    auto no = limited.read_known_open<unsigned>([](Reader &outer) {
        return outer.read_known_open<unsigned>(
            [](Reader &) { return Result<unsigned>::success(1); });
    });
    CHECK(!no && no.error().bit_offset == 3 && limited.cursor_bit() == 3 &&
          limited.usage().value_octets == 0 && limited.usage().work_bits == 3);
    // Both returned child errors and thrown child faults use the outer frame anchor.
    for (bool throws : {false, true}) {
        auto rr = take(Reader::make(e.octets));
        take(rr.read_bits(3));
        auto status = rr.read_known_open<unsigned>([&](Reader &child) -> Result<unsigned> {
            take(child.read_bits(8));
            if (throws)
                throw detail::Fault{{ErrorCode::constraint_violation, 8}};
            return Result<unsigned>::failure({ErrorCode::constraint_violation, 8});
        });
        CHECK(!status && status.error().bit_offset == 3 && rr.cursor_bit() == 3);
    }
    Writer malformed;
    take(malformed.write_bits(5, 3));
    std::byte nonzero[] = {std::byte{1}};
    take(malformed.write_open_type(nonzero));
    auto me = take(malformed.finish());
    auto mr = take(Reader::make(me.octets));
    take(mr.read_bits(3));
    auto pad = mr.read_known_open<unsigned>([](Reader &) { return Result<unsigned>::success(0); });
    CHECK(!pad && pad.error().code == ErrorCode::nonzero_padding && pad.error().bit_offset == 3 &&
          mr.cursor_bit() == 3);
}
static void oom() {
    Writer base;
    take(base.write_bits(5, 3));
    armed = true;
    allocations = 0;
    take(nested(base));
    armed = false;
    auto count = allocations;
    CHECK(count);
    auto e = take(base.finish());
    for (fail_at = 0; fail_at < count; ++fail_at) {
        Writer w;
        take(w.write_bits(5, 3));
        armed = true;
        allocations = 0;
        auto status = nested(w);
        armed = false;
        CHECK(!status && status.error().code == ErrorCode::allocation_failure &&
              w.cursor_bit() == 3 && w.usage().work_bits == 3 && !w.usage().open_staging_octets &&
              !w.usage().value_octets);
    }
    fail_at = ~std::size_t{0};
    auto r = take(Reader::make(e.octets));
    take(r.read_bits(3));
    armed = true;
    allocations = 0;
    auto value = r.read_known_open<unsigned>([](Reader &outer) {
        return outer.read_known_open<unsigned>(
            [](Reader &) { return Result<unsigned>::success(42); });
    });
    armed = false;
    CHECK(value);
    auto reads = allocations;
    CHECK(reads);
    for (fail_at = 0; fail_at < reads; ++fail_at) {
        auto rr = take(Reader::make(e.octets));
        take(rr.read_bits(3));
        armed = true;
        allocations = 0;
        auto status = rr.read_known_open<unsigned>([](Reader &outer) {
            return outer.read_known_open<unsigned>(
                [](Reader &) { return Result<unsigned>::success(42); });
        });
        armed = false;
        CHECK(!status && status.error().code == ErrorCode::allocation_failure &&
              rr.cursor_bit() == 3 && !rr.usage().value_octets && rr.usage().work_bits == 3);
    }
    fail_at = ~std::size_t{0};
    std::printf("nested open allocation rollback encode=%zu decode=%zu PASS\n", count, reads);
}
int main() {
    rollback();
    opens();
    oom();
    std::puts("UPER compound transaction/depth/work/staging/child framing PASS");
}
