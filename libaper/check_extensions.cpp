#include "runtime.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <utility>

#ifndef NDEBUG
#error "This focused test must run with NDEBUG; checks remain active."
#endif

namespace {
using namespace nrforge::aper;
bool fail_next_allocation = false;
std::size_t allocations = 0;
void require(bool condition, int line) {
    if(!condition) {
        std::fprintf(stderr, "check_extensions:%d: check failed\n", line);
        std::abort();
    }
}
#define REQUIRE(x) require(static_cast<bool>(x), __LINE__)

template<class T>
void error(const Result<T>& result, ErrorCode code, std::size_t offset) {
    REQUIRE(!result);
    if(result.error().code != code || result.error().bit_offset != offset)
        std::fprintf(stderr, "expected (%d,%zu), got (%d,%zu)\n", static_cast<int>(code),
                     offset, static_cast<int>(result.error().code), result.error().bit_offset);
    REQUIRE(result.error().code == code);
    REQUIRE(result.error().bit_offset == offset);
}

// This writer does not use runtime encoding or a decoding implementation.
// Determinants are emitted from their normative literal form; payload bits
// are generated separately and compared after framing has been removed.
struct Bits {
    std::vector<std::byte> bytes;
    std::size_t size = 0;
    void bit(bool value) {
        if(size % 8 == 0) bytes.push_back(std::byte{0});
        if(value) bytes.back() |= static_cast<std::byte>(1u << (7 - size % 8));
        ++size;
    }
    void number(std::size_t value, unsigned width) {
        for(unsigned i = width; i > 0; --i) bit(((value >> (i - 1)) & 1u) != 0);
    }
    void align() { while(size % 8 != 0) bit(false); }
    void prefix(unsigned residue) { for(unsigned i = 0; i < residue; ++i) bit((i % 2) == 0); }
    void octet(unsigned value) { number(value, 8); }
    void set(std::size_t at) { bytes[at / 8] |= static_cast<std::byte>(1u << (7 - at % 8)); }
};

std::byte payload_at(std::size_t position) {
    return static_cast<std::byte>((position * 37 + position / 251 + 0x59) % 256);
}
bool present_at(std::size_t position, unsigned pattern) {
    return pattern == 1 || (pattern == 0 && (position == 0 || position % 17 == 3));
}
void contents(Bits& bits, std::size_t offset, std::size_t count, bool bitmap, unsigned pattern) {
    for(std::size_t i = 0; i < count; ++i) {
        if(bitmap) bits.bit(present_at(offset + i, pattern));
        else bits.octet(std::to_integer<unsigned>(payload_at(offset + i)));
    }
}
void terminal(Bits& bits, std::size_t count) {
    REQUIRE(count < 16384);
    if(count < 128) bits.octet(static_cast<unsigned>(count));
    else {
        bits.octet(0x80u | static_cast<unsigned>(count >> 8));
        bits.octet(static_cast<unsigned>(count & 255));
    }
}
void long_frame(Bits& bits, std::size_t count, bool bitmap, unsigned pattern = 0) {
    std::size_t offset = 0;
    while(count - offset >= 16384) {
        const auto blocks = std::min<std::size_t>(4, (count - offset) / 16384);
        bits.octet(0xc0u + static_cast<unsigned>(blocks));
        const auto segment = blocks * 16384;
        contents(bits, offset, segment, bitmap, pattern);
        offset += segment;
    }
    terminal(bits, count - offset);
    contents(bits, offset, count - offset, bitmap, pattern);
}
Bits bitmap_frame(unsigned residue, std::size_t count, unsigned pattern = 0) {
    Bits bits;
    bits.prefix(residue);
    if(count <= 64) {
        REQUIRE(count > 0);
        bits.bit(false);
        bits.number(count - 1, 6);
        contents(bits, 0, count, true, pattern);
    } else {
        bits.bit(true);
        bits.align();
        long_frame(bits, count, true, pattern);
    }
    return bits;
}
Bits open_frame(unsigned residue, std::size_t count) {
    Bits bits;
    bits.prefix(residue);
    bits.align();
    long_frame(bits, count, false);
    return bits;
}
Limits generous() {
    Limits limits;
    limits.max_input_octets = 1u << 22;
    limits.max_wire_bits = 8u << 22;
    limits.max_extension_bitmap_bits = 1u << 20;
    limits.max_retained_unknown_payload_octets = 1u << 20;
    limits.max_retained_unknown_records = 100;
    return limits;
}
void prefix(BitReader& reader, unsigned residue) {
    for(unsigned i = 0; i < residue; ++i) {
        const auto bit = reader.read_bit();
        REQUIRE(bit && bit.value() == ((i % 2) == 0));
    }
}
void unchanged(const BitReader& reader, const DecodeContext& context, std::size_t start,
               std::size_t bitmap = 0, std::size_t payload = 0, std::size_t records = 0) {
    REQUIRE(reader.cursor_bit() == start);
    REQUIRE(context.wire_bits() == start);
    REQUIRE(context.extension_bitmap_bits() == bitmap);
    REQUIRE(context.retained_unknown_payload_octets() == payload);
    REQUIRE(context.retained_unknown_records() == records);
}

void literal_vectors() {
    REQUIRE(bitmap_frame(0, 1).bytes == std::vector<std::byte>{std::byte{0x01}});
    const auto sixty_five = bitmap_frame(0, 65, 1);
    REQUIRE(sixty_five.bytes[0] == std::byte{0x80});
    REQUIRE(sixty_five.bytes[1] == std::byte{0x41});
    REQUIRE(sixty_five.bytes.back() == std::byte{0x80});
    const auto count128 = bitmap_frame(0, 128);
    REQUIRE(count128.bytes[1] == std::byte{0x80} && count128.bytes[2] == std::byte{0x80});
    const auto one = open_frame(0, 1);
    REQUIRE(one.bytes == (std::vector<std::byte>{std::byte{1}, std::byte{0x59}}));
    const auto fragment = open_frame(0, 16384);
    REQUIRE(fragment.bytes.front() == std::byte{0xc1} && fragment.bytes.back() == std::byte{0});
}

void valid_vectors() {
    constexpr std::array<std::size_t, 15> bitmap_counts{
        1, 2, 7, 8, 63, 64, 65, 127, 128, 255, 16383, 16384, 32768, 49152, 65537};
    constexpr std::array<std::size_t, 12> payload_counts{
        1, 2, 127, 128, 16383, 16384, 16385, 32768, 49152, 65536, 65537, 131072};
    for(unsigned residue = 0; residue < 8; ++residue) {
        for(auto count : bitmap_counts) {
            for(unsigned pattern : {0u, 1u}) {
                auto bits = bitmap_frame(residue, count, pattern);
                DecodeContext context(generous());
                auto made = BitReader::make(bits.bytes, context);
                REQUIRE(made);
                auto reader = std::move(made).value();
                prefix(reader, residue);
                FieldReader fields(reader);
                auto result = fields.read_sequence_extension_bitmap();
                REQUIRE(result && result.value().bit_count == count);
                const auto& packed = result.value().packed_bits;
                REQUIRE(packed.size() == (count + 7) / 8);
                for(std::size_t i = 0; i < count; ++i)
                    REQUIRE(((std::to_integer<unsigned>(packed[i / 8]) >> (7 - i % 8)) & 1u)
                            == static_cast<unsigned>(present_at(i, pattern)));
                if(count % 8) REQUIRE((std::to_integer<unsigned>(packed.back()) &
                                      ((1u << (8 - count % 8)) - 1)) == 0);
                REQUIRE(reader.cursor_bit() == bits.size && context.wire_bits() == bits.size);
                REQUIRE(context.extension_bitmap_bits() == count);
                REQUIRE(context.retained_unknown_records() == 0);
                REQUIRE(reader.validate_complete_value());
                // Ownership and copy semantics after the wire input changes.
                auto copy = result.value();
                std::fill(bits.bytes.begin(), bits.bytes.end(), std::byte{0});
                REQUIRE(copy.packed_bits == result.value().packed_bits);
                copy.packed_bits[0] ^= std::byte{0x80};
                REQUIRE(copy.packed_bits != result.value().packed_bits);
            }
        }
        for(auto count : payload_counts) {
            auto bits = open_frame(residue, count);
            DecodeContext context(generous());
            auto made = BitReader::make(bits.bytes, context);
            REQUIRE(made);
            auto reader = std::move(made).value();
            prefix(reader, residue);
            FieldReader fields(reader);
            auto result = fields.read_open_type_owned();
            REQUIRE(result && result.value().size() == count);
            for(std::size_t i = 0; i < count; ++i) REQUIRE(result.value()[i] == payload_at(i));
            REQUIRE(reader.cursor_bit() == bits.size && context.wire_bits() == bits.size);
            REQUIRE(context.retained_unknown_payload_octets() == count);
            REQUIRE(context.retained_unknown_records() == 1);
            REQUIRE(context.extension_bitmap_bits() == 0);
            REQUIRE(reader.validate_complete_value());
            auto copy = result.value();
            bits.bytes.clear();
            bits.bytes.shrink_to_fit();
            REQUIRE(result.value()[count - 1] == payload_at(count - 1));
            copy[0] ^= std::byte{0xff};
            REQUIRE(copy[0] != result.value()[0]);
        }
    }
}

void budgets_and_truncation() {
    for(bool bitmap : {false, true}) {
        for(unsigned residue = 0; residue < 8; ++residue) {
            for(auto count : {std::size_t{1}, std::size_t{64}, std::size_t{65},
                              std::size_t{128}, std::size_t{16384}, std::size_t{65537}}) {
                const auto bits = bitmap ? bitmap_frame(residue, count) : open_frame(residue, count);
                for(unsigned budget = 0; budget < (bitmap ? 3u : 4u); ++budget) {
                    for(bool short_limit : {false, true}) {
                        auto limits = generous();
                        const auto less = static_cast<std::size_t>(short_limit);
                        if(budget == 0) limits.max_input_octets = bits.bytes.size() - less;
                        if(budget == 1) limits.max_wire_bits = bits.size - less;
                        if(budget == 2) {
                            if(bitmap) limits.max_extension_bitmap_bits = count - less;
                            else limits.max_retained_unknown_payload_octets = count - less;
                        }
                        if(budget == 3) limits.max_retained_unknown_records = 1 - less;
                        DecodeContext context(limits);
                        auto made = BitReader::make(bits.bytes, context);
                        if(budget == 0 && short_limit) {
                            error(made, ErrorCode::resource_limit, 0);
                            continue;
                        }
                        REQUIRE(made);
                        auto reader = std::move(made).value();
                        prefix(reader, residue);
                        if(bitmap) {
                            const auto result = reader.read_sequence_extension_bitmap();
                            if(short_limit) error(result, ErrorCode::resource_limit, residue);
                            else REQUIRE(result);
                        } else {
                            const auto result = reader.read_open_type_owned();
                            if(short_limit) error(result, ErrorCode::resource_limit, residue);
                            else REQUIRE(result);
                        }
                        if(short_limit) unchanged(reader, context, residue);
                    }
                }
                // Exhaustive small bit truncations; selected large-fragment boundaries
                // avoid a quadratic scan of a 64K payload for every possible cut.
                std::vector<std::size_t> cuts;
                if(bits.size < 1200) {
                    for(std::size_t cut = residue; cut < bits.size; ++cut) cuts.push_back(cut);
                } else {
                    cuts = {residue, residue + 1, bits.size - 1, bits.size - 8};
                    const auto aligned = (static_cast<std::size_t>(residue) + (bitmap ? 1 : 0) + 7) / 8 * 8;
                    if(aligned + 8 < bits.size) cuts.push_back(aligned + 8);
                    const auto segment_end = aligned + 8 + 16384 * (bitmap ? 1 : 8);
                    if(segment_end < bits.size) {
                        cuts.push_back(segment_end - 1);
                        cuts.push_back(segment_end);
                        cuts.push_back(segment_end + 1);
                    }
                }
                for(auto cut : cuts) {
                    auto limits = generous();
                    limits.max_wire_bits = residue;
                    limits.max_extension_bitmap_bits = 0;
                    limits.max_retained_unknown_payload_octets = 0;
                    limits.max_retained_unknown_records = 0;
                    DecodeContext context(limits);
                    auto made = BitReader::make_bounded_for_test(bits.bytes, cut, context);
                    REQUIRE(made);
                    auto reader = std::move(made).value();
                    prefix(reader, residue);
                    if(bitmap) error(reader.read_sequence_extension_bitmap(), ErrorCode::truncated_input, cut);
                    else error(reader.read_open_type_owned(), ErrorCode::truncated_input, cut);
                    unchanged(reader, context, residue);
                    error(reader.read_bit(), ErrorCode::truncated_input, cut);
                }
            }
        }
    }
}

void invalid_frame(const Bits& bits, unsigned residue, bool bitmap, ErrorCode code,
                   std::size_t offset, Limits limits = generous()) {
    DecodeContext context(limits);
    auto made = BitReader::make_bounded_for_test(bits.bytes, bits.size, context);
    REQUIRE(made);
    auto reader = std::move(made).value();
    prefix(reader, residue);
    if(bitmap) error(reader.read_sequence_extension_bitmap(), code, offset);
    else error(reader.read_open_type_owned(), code, offset);
    unchanged(reader, context, residue);
    error(reader.read_open_type_owned(), code, offset);
    error(reader.read_sequence_extension_bitmap(), code, offset);
}

void malformed() {
    for(unsigned residue = 0; residue < 8; ++residue) {
        for(bool bitmap : {false, true}) {
            Bits base;
            base.prefix(residue);
            if(bitmap) base.bit(true);
            const auto padding = base.size;
            base.align();
            const auto determinant = base.size;
            for(unsigned selector = 0xc0; selector <= 0xff; ++selector) {
                if(selector >= 0xc1 && selector <= 0xc4) continue;
                auto bad = base;
                bad.octet(selector);
                auto limits = generous();
                limits.max_wire_bits = residue;
                invalid_frame(bad, residue, bitmap, ErrorCode::constraint_violation, determinant, limits);
            }
            auto zero = base;
            zero.octet(0);
            invalid_frame(zero, residue, bitmap, ErrorCode::constraint_violation, residue);
            for(std::size_t length : {std::size_t{1}, std::size_t{64}, std::size_t{127}}) {
                auto overlong = base;
                overlong.octet(0x80);
                overlong.octet(static_cast<unsigned>(length));
                contents(overlong, 0, length, bitmap, 0);
                invalid_frame(overlong, residue, bitmap, ErrorCode::constraint_violation, determinant);
                auto limits = generous();
                limits.max_wire_bits = residue;
                invalid_frame(overlong, residue, bitmap, ErrorCode::resource_limit, residue, limits);
            }
            if(bitmap) {
                auto long_short = base;
                long_short.octet(64);
                contents(long_short, 0, 64, true, 0);
                invalid_frame(long_short, residue, true, ErrorCode::constraint_violation, residue);
                auto absent = bitmap_frame(residue, 65, 2);
                invalid_frame(absent, residue, true, ErrorCode::constraint_violation, residue);
                auto absent_fragment = bitmap_frame(residue, 16384, 2);
                invalid_frame(absent_fragment, residue, true, ErrorCode::constraint_violation, residue);
                auto absent_short = bitmap_frame(residue, 3, 2);
                invalid_frame(absent_short, residue, true, ErrorCode::constraint_violation, residue);
            }
            if(padding < determinant) {
                auto bad_padding = bitmap ? bitmap_frame(residue, 128) : open_frame(residue, 128);
                bad_padding.set(padding);
                invalid_frame(bad_padding, residue, bitmap, ErrorCode::nonzero_padding, padding);
                auto limits = generous();
                limits.max_wire_bits = residue;
                invalid_frame(bad_padding, residue, bitmap, ErrorCode::resource_limit, residue, limits);
                // Padding must precede overlong form, after a fully readable frame.
                auto both = base;
                both.octet(0x80);
                both.octet(1);
                contents(both, 0, 1, bitmap, 0);
                both.set(padding);
                invalid_frame(both, residue, bitmap, ErrorCode::nonzero_padding, padding);
            }
            // A complete but non-maximal C1+C1 must be C2. Frame boundary is
            // established before budgets/canonicality, even at the second fragment.
            auto nonmax = base;
            nonmax.octet(0xc1);
            contents(nonmax, 0, 16384, bitmap, 0);
            nonmax.octet(0xc1);
            contents(nonmax, 16384, 16384, bitmap, 0);
            nonmax.octet(0);
            invalid_frame(nonmax, residue, bitmap, ErrorCode::constraint_violation, determinant);
            auto limits = generous();
            limits.max_wire_bits = residue;
            invalid_frame(nonmax, residue, bitmap, ErrorCode::resource_limit, residue, limits);
            nonmax.size -= 8;
            invalid_frame(nonmax, residue, bitmap, ErrorCode::truncated_input, nonmax.size, limits);
        }
    }
}

void shared_budgets_and_allocation() {
    for(bool fail_allocation : {false, true}) {
        Bits bits = bitmap_frame(0, 3);
        bits.align();
        long_frame(bits, 1, false);
        const auto start = bits.size;
        long_frame(bits, 65536, false);
        auto limits = generous();
        limits.max_extension_bitmap_bits = 3;
        limits.max_retained_unknown_payload_octets = fail_allocation ? 65537 : 65536;
        limits.max_retained_unknown_records = 2;
        DecodeContext context(limits);
        auto made = BitReader::make(bits.bytes, context);
        REQUIRE(made);
        auto reader = std::move(made).value();
        REQUIRE(reader.read_sequence_extension_bitmap());
        REQUIRE(reader.read_open_type_owned());
        if(fail_allocation) fail_next_allocation = true;
        auto result = reader.read_open_type_owned();
        error(result, fail_allocation ? ErrorCode::allocation_failure : ErrorCode::resource_limit, start);
        REQUIRE(!fail_next_allocation);
        unchanged(reader, context, start, 3, 1, 1);
    }
    // Two bitmaps consume a shared received-width budget, including trailing absences.
    Bits bits = bitmap_frame(0, 3);
    bits.bit(false);
    bits.number(1, 6);
    contents(bits, 0, 2, true, 0);
    auto limits = generous();
    limits.max_extension_bitmap_bits = 4;
    DecodeContext context(limits);
    auto made = BitReader::make(bits.bytes, context);
    REQUIRE(made);
    auto reader = std::move(made).value();
    REQUIRE(reader.read_sequence_extension_bitmap());
    const auto start = reader.cursor_bit();
    error(reader.read_sequence_extension_bitmap(), ErrorCode::resource_limit, start);
    unchanged(reader, context, start, 3);

    for(bool bitmap : {false, true}) {
        auto frame = bitmap ? bitmap_frame(3, 65537) : open_frame(3, 65537);
        DecodeContext owned_context(generous());
        auto owner = BitReader::make(frame.bytes, owned_context);
        REQUIRE(owner);
        auto owned_reader = std::move(owner).value();
        prefix(owned_reader, 3);
        const auto before = allocations;
        fail_next_allocation = true;
        if(bitmap) error(owned_reader.read_sequence_extension_bitmap(), ErrorCode::allocation_failure, 3);
        else error(owned_reader.read_open_type_owned(), ErrorCode::allocation_failure, 3);
        REQUIRE(!fail_next_allocation && allocations == before + 1);
        unchanged(owned_reader, owned_context, 3);
    }
}

void cumulative_and_late_failures() {
    // Freeze backwards-compatible aggregate member order and default budgets.
    constexpr Limits original_three{17, 23, 31};
    static_assert(original_three.max_input_octets == 17);
    static_assert(original_three.max_output_octets == 23);
    static_assert(original_three.max_wire_bits == 31);
    static_assert(original_three.max_extension_bitmap_bits == 1024);
    static_assert(original_three.max_retained_unknown_payload_octets == (1u << 20));
    static_assert(original_three.max_retained_unknown_records == 1024);
    for(unsigned mode = 0; mode < 5; ++mode) {
        auto bits = open_frame(0, 1);
        const auto start = bits.size;
        const auto second_header = bits.size;
        long_frame(bits, 16384, false);
        auto limits = generous();
        if(mode == 0) limits.max_retained_unknown_records = 1;
        if(mode == 1) limits.max_retained_unknown_records = 2;
        if(mode == 2) bits.size -= 8; // Missing exact-fragment terminator, after an earlier commit.
        if(mode == 3) { // Fully readable illegal selector after a successful record.
            bits.bytes[second_header / 8] = std::byte{0xc0};
            limits.max_retained_unknown_records = 1;
        }
        if(mode == 4) { // Invalid late selector after a full legal fragment.
            bits.bytes.back() = std::byte{0xff};
            limits.max_retained_unknown_records = 1;
        }
        DecodeContext context(limits);
        auto made = BitReader::make_bounded_for_test(bits.bytes, bits.size, context);
        REQUIRE(made);
        auto reader = std::move(made).value();
        REQUIRE(reader.read_open_type_owned());
        const auto result = reader.read_open_type_owned();
        if(mode == 1) {
            REQUIRE(result && context.retained_unknown_records() == 2);
            REQUIRE(context.retained_unknown_payload_octets() == 16385);
            continue;
        }
        const auto code = mode == 0 ? ErrorCode::resource_limit :
            mode == 2 ? ErrorCode::truncated_input : ErrorCode::constraint_violation;
        const auto offset = mode == 2 ? bits.size : mode == 4 ? bits.size - 8 : start;
        error(result, code, offset);
        unchanged(reader, context, start, 0, 1, 1);
        error(reader.read_constrained_uint(7), code, offset); // Sticky beats new invalid argument.
        error(reader.validate_complete_value(), code, offset);
    }
    auto bits = bitmap_frame(0, 1);
    bits.bit(false);
    DecodeContext context(generous());
    auto made = BitReader::make_bounded_for_test(bits.bytes, bits.size, context);
    REQUIRE(made);
    auto reader = std::move(made).value();
    REQUIRE(reader.read_sequence_extension_bitmap());
    REQUIRE(reader.read_bit());
    REQUIRE(context.extension_bitmap_bits() == 1 && context.retained_unknown_payload_octets() == 0
            && context.retained_unknown_records() == 0);
    // Prior ordinary primitive failure is retained by both new methods.
    error(reader.read_bit(), ErrorCode::truncated_input, bits.size);
    error(reader.read_sequence_extension_bitmap(), ErrorCode::truncated_input, bits.size);
    error(reader.read_open_type_owned(), ErrorCode::truncated_input, bits.size);
}

void lifecycle_and_complete() {
    auto bits = open_frame(0, 1);
    DecodeContext context(generous());
    auto made = BitReader::make(bits.bytes, context);
    REQUIRE(made);
    auto reader = std::move(made).value();
    auto moved = std::move(reader);
    error(reader.read_open_type_owned(), ErrorCode::invalid_state, 0);
    error(reader.read_sequence_extension_bitmap(), ErrorCode::invalid_state, 0);
    REQUIRE(!context.failed());
    REQUIRE(moved.read_open_type_owned());
    REQUIRE(moved.validate_complete_value());
    error(moved.read_open_type_owned(), ErrorCode::invalid_state, bits.size);
    error(moved.read_sequence_extension_bitmap(), ErrorCode::invalid_state, bits.size);
    REQUIRE(!context.failed());

    EncodeContext encode;
    BitWriter writer(encode);
    REQUIRE(writer.write_bit(true));
    FieldWriter fields(writer);
    error(fields.reject_sequence_extension_data(), ErrorCode::constraint_violation, 1);
    REQUIRE(writer.cursor_bit() == 1 && encode.wire_bits() == 1 && encode.logical_output_octets() == 1);
    error(writer.write_bit(false), ErrorCode::constraint_violation, 1);
    error(writer.reject_sequence_extension_data(), ErrorCode::constraint_violation, 1);
    error(writer.finish(), ErrorCode::constraint_violation, 1);
    EncodeContext finished_context;
    BitWriter finished(finished_context);
    REQUIRE(finished.finish());
    REQUIRE(finished.cursor_bit() == 8);
    error(finished.reject_sequence_extension_data(), ErrorCode::invalid_state, finished.cursor_bit());
    REQUIRE(!finished_context.failed());
    EncodeContext move_context;
    BitWriter original(move_context);
    BitWriter transferred(std::move(original));
    error(original.reject_sequence_extension_data(), ErrorCode::invalid_state, 0);
    REQUIRE(!move_context.failed());
    error(transferred.reject_sequence_extension_data(), ErrorCode::constraint_violation, 0);

    const std::array<std::byte, 1> empty_open{std::byte{0}};
    auto ignored = decode_complete<int>(empty_open, generous(), [](FieldReader& input) {
        (void)input.read_open_type_owned();
        return Result<int>::success(1);
    });
    error(ignored, ErrorCode::constraint_violation, 0);
    auto thrown = decode_complete<int>(empty_open, generous(), [](FieldReader& input) -> Result<int> {
        (void)input.read_open_type_owned();
        throw std::bad_alloc();
    });
    error(thrown, ErrorCode::constraint_violation, 0);
    auto rejected = encode_complete<int>(0, Limits{}, [](FieldWriter& output) {
        (void)output.reject_sequence_extension_data();
        return Result<void>::success();
    });
    error(rejected, ErrorCode::constraint_violation, 0);
    // Inner zero byte is preserved and is not a fresh complete decode.
    const std::array<std::byte, 2> substituted{std::byte{1}, std::byte{0}};
    auto zero_inner = decode_complete<std::vector<std::byte>>(substituted, generous(),
        [](FieldReader& input) { return input.read_open_type_owned(); });
    REQUIRE(zero_inner && zero_inner.value() == std::vector<std::byte>{std::byte{0}});
    const std::array<std::byte, 3> trailing{std::byte{1}, std::byte{0}, std::byte{0}};
    auto trailing_result = decode_complete<std::vector<std::byte>>(trailing, generous(),
        [](FieldReader& input) { return input.read_open_type_owned(); });
    error(trailing_result, ErrorCode::trailing_data, 16);
}
} // namespace

void* operator new(std::size_t size) {
    ++allocations;
    if(fail_next_allocation) { fail_next_allocation = false; throw std::bad_alloc(); }
    if(void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

int main() {
    literal_vectors();
    valid_vectors();
    budgets_and_truncation();
    malformed();
    shared_budgets_and_allocation();
    cumulative_and_late_failures();
    lifecycle_and_complete();
    return 0;
}
