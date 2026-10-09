#include "runtime.hpp"

#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>
#include <type_traits>

namespace {
bool fail_next_allocation = false;

void require(bool condition, int line) {
    if(!condition) {
        std::fprintf(stderr, "check_runtime:%d: assertion failed\n", line);
        std::abort();
    }
}

#define REQUIRE(expr) require(static_cast<bool>(expr), __LINE__)

using nrforge::aper::BitReader;
using nrforge::aper::BitWriter;
using nrforge::aper::CompleteEncoding;
using nrforge::aper::DecodeContext;
using nrforge::aper::EncodeContext;
using nrforge::aper::ErrorCode;
using nrforge::aper::FieldReader;
using nrforge::aper::FieldWriter;
using nrforge::aper::Limits;
using nrforge::aper::Result;

template<class T>
void expect_error(const Result<T>& result, ErrorCode code, std::size_t offset) {
    if(result) std::fprintf(stderr, "expected error %d at %zu, got success\n",
                            static_cast<int>(code), offset);
    REQUIRE(!result);
    if(result.error().code != code || result.error().bit_offset != offset)
        std::fprintf(stderr, "expected error %d at %zu, got %d at %zu\n",
                     static_cast<int>(code), offset, static_cast<int>(result.error().code),
                     result.error().bit_offset);
    REQUIRE(result.error().code == code);
    REQUIRE(result.error().bit_offset == offset);
}

std::array<std::byte, 2> encode_u16(std::uint64_t value) {
    EncodeContext context;
    BitWriter writer(context);
    auto wrote = writer.write_aligned_u16_be(value);
    REQUIRE(wrote);
    auto result = writer.finish();
    REQUIRE(result);
    REQUIRE(result.value().octets.size() == 2);
    return {result.value().octets[0], result.value().octets[1]};
}

void test_bit_order_and_boolean_selector() {
    EncodeContext context;
    BitWriter writer(context);
    const bool bits[] = {true, false, true, true, false, false, true, false,
                         true, false, true};
    for(bool bit : bits) REQUIRE(writer.write_bit(bit));
    auto result = writer.finish();
    REQUIRE(result);
    const std::array<std::byte, 2> expected{std::byte{0xB2}, std::byte{0xA0}};
    REQUIRE(result.value().octets.size() == expected.size());
    REQUIRE(result.value().octets[0] == expected[0]);
    REQUIRE(result.value().octets[1] == expected[1]);
    REQUIRE(result.value().last_field_end_bit == 11);
    REQUIRE(result.value().final_padding_bits == 5);
    REQUIRE(result.value().complete_encoding_bits == 16);

    DecodeContext decode_context;
    auto reader_result = BitReader::make(result.value().octets, decode_context);
    REQUIRE(reader_result);
    auto reader = std::move(reader_result).value();
    for(bool bit : bits) {
        auto read = reader.read_bit();
        REQUIRE(read && read.value() == bit);
    }
    REQUIRE(reader.validate_complete_value());
}

void test_u16_values_and_alignment_residues() {
    const std::uint64_t values[] = {0, 1, 255, 256, 65535};
    const std::array<std::array<std::byte, 2>, 5> expected{{
        {std::byte{0x00}, std::byte{0x00}}, {std::byte{0x00}, std::byte{0x01}},
        {std::byte{0x00}, std::byte{0xFF}}, {std::byte{0x01}, std::byte{0x00}},
        {std::byte{0xFF}, std::byte{0xFF}},
    }};
    for(std::size_t i = 0; i < 5; ++i) REQUIRE(encode_u16(values[i]) == expected[i]);

    for(std::size_t residue = 0; residue < 8; ++residue) {
        EncodeContext context;
        BitWriter writer(context);
        for(std::size_t i = 0; i < residue; ++i) REQUIRE(writer.write_bit(true));
        REQUIRE(writer.write_aligned_u16_be(0x1234));
        auto encoded = writer.finish();
        REQUIRE(encoded);
        if(residue == 0) {
            REQUIRE(encoded.value().octets.size() == 2);
            REQUIRE(encoded.value().octets[0] == std::byte{0x12});
            REQUIRE(encoded.value().octets[1] == std::byte{0x34});
        } else {
            const auto prefix = static_cast<unsigned char>(0xFFu << (8 - residue));
            REQUIRE(encoded.value().octets.size() == 3);
            REQUIRE(encoded.value().octets[0] == static_cast<std::byte>(prefix));
            REQUIRE(encoded.value().octets[1] == std::byte{0x12});
            REQUIRE(encoded.value().octets[2] == std::byte{0x34});
        }
        REQUIRE(context.wire_bits() == residue + ((8 - residue) % 8) + 16);

        DecodeContext decode_context;
        auto made = BitReader::make(encoded.value().octets, decode_context);
        REQUIRE(made);
        auto reader = std::move(made).value();
        for(std::size_t i = 0; i < residue; ++i) {
            auto prefix_bit = reader.read_bit();
            REQUIRE(prefix_bit && prefix_bit.value());
        }
        auto decoded = reader.read_aligned_u16_be();
        REQUIRE(decoded && decoded.value() == 0x1234);
        REQUIRE(reader.validate_complete_value());
    }
}

void test_range_rejection_is_atomic_and_sticky() {
    for(const auto value : {std::uint64_t{65536}, std::uint64_t{1} << 32,
                            UINT64_MAX}) {
        EncodeContext context;
        BitWriter writer(context);
        REQUIRE(writer.write_bit(true));
        const auto before_wire = context.wire_bits();
        auto failed = writer.write_aligned_u16_be(value);
        expect_error(failed, ErrorCode::constraint_violation, 1);
        REQUIRE(writer.cursor_bit() == 1);
        REQUIRE(context.wire_bits() == before_wire);
        auto repeated = writer.write_bit(false);
        expect_error(repeated, ErrorCode::constraint_violation, 1);
        expect_error(writer.write_aligned_u16_be(UINT64_MAX),
                     ErrorCode::constraint_violation, 1);
        auto finish = writer.finish();
        expect_error(finish, ErrorCode::constraint_violation, 1);
    }
}

void test_truncation_and_alignment_atomicity() {
    const std::array<std::byte, 3> bytes{std::byte{0x80}, std::byte{0x12}, std::byte{0x34}};
    for(std::size_t limit = 1; limit < 24; ++limit) {
        DecodeContext context;
        auto made = BitReader::make_bounded_for_test(bytes, limit, context);
        REQUIRE(made);
        auto reader = std::move(made).value();
        REQUIRE(reader.read_bit());
        const auto before_cursor = reader.cursor_bit();
        const auto before_wire = context.wire_bits();
        auto failed = reader.read_aligned_u16_be();
        expect_error(failed, ErrorCode::truncated_input, limit);
        REQUIRE(reader.cursor_bit() == before_cursor);
        REQUIRE(context.wire_bits() == before_wire);
        auto repeated = reader.read_bit();
        expect_error(repeated, ErrorCode::truncated_input, limit);
    }

    const std::array<std::byte, 3> bad_alignment{
        std::byte{0xC0}, std::byte{0x00}, std::byte{0x00}};
    DecodeContext context;
    auto made = BitReader::make(bad_alignment, context);
    REQUIRE(made);
    auto reader = std::move(made).value();
    REQUIRE(reader.read_bit());
    auto bad = reader.read_aligned_u16_be();
    expect_error(bad, ErrorCode::nonzero_padding, 1);
    REQUIRE(reader.cursor_bit() == 1);
    REQUIRE(context.wire_bits() == 1);
}

void test_nonzero_final_padding_trailing_and_empty() {
    const std::array<std::byte, 1> bad_final{std::byte{0x81}};
    DecodeContext context;
    auto made = BitReader::make(bad_final, context);
    REQUIRE(made);
    auto reader = std::move(made).value();
    REQUIRE(reader.read_bit());
    auto invalid = reader.validate_complete_value();
    expect_error(invalid, ErrorCode::nonzero_padding, 7);
    REQUIRE(reader.cursor_bit() == 1);
    REQUIRE(context.wire_bits() == 1);

    const std::array<std::byte, 2> trailing{std::byte{0x80}, std::byte{0x00}};
    DecodeContext trailing_context;
    auto trailing_made = BitReader::make(trailing, trailing_context);
    REQUIRE(trailing_made);
    auto trailing_reader = std::move(trailing_made).value();
    REQUIRE(trailing_reader.read_bit());
    expect_error(trailing_reader.validate_complete_value(), ErrorCode::trailing_data, 8);
    REQUIRE(trailing_reader.cursor_bit() == 1);
    REQUIRE(trailing_context.wire_bits() == 1);

    EncodeContext empty_context;
    BitWriter empty_writer(empty_context);
    auto empty = empty_writer.finish();
    REQUIRE(empty);
    REQUIRE(empty.value().octets == std::vector<std::byte>{std::byte{0}});
    REQUIRE(empty.value().last_field_end_bit == 0);
    REQUIRE(empty.value().final_padding_bits == 0);
    REQUIRE(empty.value().empty_encoding_substitution);
    REQUIRE(empty.value().complete_encoding_bits == 8);
    REQUIRE(empty.value().octet_count == 1);
    REQUIRE(empty_context.wire_bits() == 8);
    REQUIRE(empty_context.logical_output_octets() == 1);
    expect_error(empty_writer.finish(), ErrorCode::invalid_state, 8);

    const std::array<std::byte, 1> zero{std::byte{0}};
    auto decoded_zero = nrforge::aper::decode_complete<int>(zero, Limits{},
        [](FieldReader&) { return Result<int>::success(7); });
    REQUIRE(decoded_zero && decoded_zero.value() == 7);
    expect_error(nrforge::aper::decode_complete<int>({}, Limits{},
        [](FieldReader&) { return Result<int>::success(7); }), ErrorCode::truncated_input, 0);
    const std::array<std::byte, 1> nonzero{std::byte{0x01}};
    expect_error(nrforge::aper::decode_complete<int>(nonzero, Limits{},
        [](FieldReader&) { return Result<int>::success(7); }), ErrorCode::nonzero_padding, 7);
    const std::array<std::byte, 2> zero_trailing{std::byte{0}, std::byte{0}};
    expect_error(nrforge::aper::decode_complete<int>(zero_trailing, Limits{},
        [](FieldReader&) { return Result<int>::success(7); }), ErrorCode::trailing_data, 8);
}

void test_complete_wrappers_and_budgets() {
    auto encoded = nrforge::aper::encode_complete<int>(42, Limits{},
        [](FieldWriter& fields) {
            auto result = fields.write_bit(true);
            if(!result) return result;
            return Result<void>::success();
        });
    REQUIRE(encoded && encoded.value().octets[0] == std::byte{0x80});
    const std::array<std::byte, 1> one{std::byte{0x80}};
    auto decoded = nrforge::aper::decode_complete<int>(one, Limits{},
        [](FieldReader& fields) {
            auto result = fields.read_bit();
            if(!result) return Result<int>::failure(result.error());
            return Result<int>::success(result.value() ? 1 : 0);
        });
    REQUIRE(decoded && decoded.value() == 1);

    auto ignored = nrforge::aper::encode_complete<int>(0, Limits{},
        [](FieldWriter& fields) {
            (void)fields.write_aligned_u16_be(65536);
            return Result<void>::success();
        });
    expect_error(ignored, ErrorCode::constraint_violation, 0);

    Limits exact;
    exact.max_output_octets = 1;
    exact.max_wire_bits = 8;
    auto exact_output = nrforge::aper::encode_complete<int>(0, exact,
        [](FieldWriter& fields) { return fields.write_bit(true); });
    REQUIRE(exact_output);
    Limits short_output = exact;
    short_output.max_output_octets = 0;
    expect_error(nrforge::aper::encode_complete<int>(0, short_output,
        [](FieldWriter& fields) { return fields.write_bit(true); }), ErrorCode::resource_limit, 0);
    Limits short_wire = exact;
    short_wire.max_wire_bits = 7;
    expect_error(nrforge::aper::encode_complete<int>(0, short_wire,
        [](FieldWriter& fields) { return fields.write_bit(true); }), ErrorCode::resource_limit, 1);

    Limits exact_empty;
    exact_empty.max_output_octets = 1;
    exact_empty.max_wire_bits = 8;
    REQUIRE(nrforge::aper::encode_complete<int>(0, exact_empty,
        [](FieldWriter&) { return Result<void>::success(); }));
    exact_empty.max_wire_bits = 7;
    expect_error(nrforge::aper::encode_complete<int>(0, exact_empty,
        [](FieldWriter&) { return Result<void>::success(); }), ErrorCode::resource_limit, 0);
    exact_empty.max_wire_bits = 8;
    exact_empty.max_output_octets = 0;
    expect_error(nrforge::aper::encode_complete<int>(0, exact_empty,
        [](FieldWriter&) { return Result<void>::success(); }), ErrorCode::resource_limit, 0);

    Limits input_exact;
    input_exact.max_input_octets = 1;
    REQUIRE(nrforge::aper::decode_complete<int>(one, input_exact,
        [](FieldReader& fields) {
            auto bit = fields.read_bit();
            return bit ? Result<int>::success(1) : Result<int>::failure(bit.error());
        }));
    input_exact.max_input_octets = 0;
    expect_error(nrforge::aper::decode_complete<int>(one, input_exact,
        [](FieldReader& fields) {
            auto bit = fields.read_bit();
            return bit ? Result<int>::success(1) : Result<int>::failure(bit.error());
        }), ErrorCode::resource_limit, 0);

    Limits exact_decode;
    exact_decode.max_wire_bits = 8;
    REQUIRE(nrforge::aper::decode_complete<int>(one, exact_decode,
        [](FieldReader& fields) {
            auto bit = fields.read_bit();
            return bit ? Result<int>::success(1) : Result<int>::failure(bit.error());
        }));
    exact_decode.max_wire_bits = 7;
    expect_error(nrforge::aper::decode_complete<int>(one, exact_decode,
        [](FieldReader& fields) {
            auto bit = fields.read_bit();
            return bit ? Result<int>::success(1) : Result<int>::failure(bit.error());
        }), ErrorCode::resource_limit, 1);
}

void test_allocation_failure_atomicity() {
    EncodeContext context;
    BitWriter writer(context);
    fail_next_allocation = true;
    auto failed = writer.write_bit(true);
    expect_error(failed, ErrorCode::allocation_failure, 0);
    REQUIRE(writer.cursor_bit() == 0);
    REQUIRE(context.wire_bits() == 0);
    REQUIRE(context.logical_output_octets() == 0);
    expect_error(writer.write_bit(false), ErrorCode::allocation_failure, 0);

    EncodeContext finish_context;
    BitWriter empty_writer(finish_context);
    fail_next_allocation = true;
    auto finish_failure = empty_writer.finish();
    expect_error(finish_failure, ErrorCode::allocation_failure, 0);
    REQUIRE(empty_writer.cursor_bit() == 0);
    REQUIRE(finish_context.wire_bits() == 0);
    REQUIRE(finish_context.logical_output_octets() == 0);
    expect_error(empty_writer.finish(), ErrorCode::allocation_failure, 0);

    auto prior_encode_error = nrforge::aper::encode_complete<int>(0, Limits{},
        [](FieldWriter& fields) -> Result<void> {
            (void)fields.write_aligned_u16_be(65536);
            throw std::bad_alloc();
        });
    expect_error(prior_encode_error, ErrorCode::constraint_violation, 0);

    auto prior_decode_error = nrforge::aper::decode_complete<int>({}, Limits{},
        [](FieldReader& fields) -> Result<int> {
            (void)fields.read_bit();
            throw std::bad_alloc();
        });
    expect_error(prior_decode_error, ErrorCode::truncated_input, 0);

    auto encode_allocation = nrforge::aper::encode_complete<int>(0, Limits{},
        [](FieldWriter&) {
            std::vector<int> allocation_probe;
            fail_next_allocation = true;
            allocation_probe.push_back(1);
            return Result<void>::success();
        });
    expect_error(encode_allocation, ErrorCode::allocation_failure, 0);

    const std::array<std::byte, 1> input{std::byte{0}};
    auto decode_allocation = nrforge::aper::decode_complete<int>(input, Limits{},
        [](FieldReader& fields) {
            auto bit = fields.read_bit();
            if(!bit) return Result<int>::failure(bit.error());
            std::vector<int> allocation_probe;
            fail_next_allocation = true;
            allocation_probe.push_back(1);
            return Result<int>::success(1);
        });
    expect_error(decode_allocation, ErrorCode::allocation_failure, 1);
}

void test_result_types_and_invalid_state() {
    static_assert(!std::is_copy_constructible_v<BitReader>);
    static_assert(!std::is_copy_constructible_v<BitWriter>);
    static_assert(!std::is_copy_constructible_v<FieldReader>);
    static_assert(!std::is_copy_constructible_v<FieldWriter>);
    EncodeContext context;
    BitWriter writer(context);
    REQUIRE(writer.write_bit(false));
    auto finished = writer.finish();
    REQUIRE(finished);
    const auto writer_wire = context.wire_bits();
    const auto writer_octets = context.logical_output_octets();
    auto writer_invalid = writer.write_bit(true);
    expect_error(writer_invalid, ErrorCode::invalid_state, 8);
    expect_error(writer.write_aligned_u16_be(UINT64_MAX), ErrorCode::invalid_state, 8);
    expect_error(writer.align_to_octet_zero(), ErrorCode::invalid_state, 8);
    expect_error(writer.finish(), ErrorCode::invalid_state, 8);
    REQUIRE(context.finished() && !context.failed());
    REQUIRE(context.wire_bits() == writer_wire);
    REQUIRE(context.logical_output_octets() == writer_octets);
    REQUIRE(writer.cursor_bit() == 8);

    EncodeContext moved_writer_context;
    BitWriter writer_source(moved_writer_context);
    BitWriter writer_destination(std::move(writer_source));
    expect_error(writer_source.write_bit(true), ErrorCode::invalid_state, 0);
    expect_error(writer_source.write_aligned_u16_be(UINT64_MAX), ErrorCode::invalid_state, 0);
    expect_error(writer_source.align_to_octet_zero(), ErrorCode::invalid_state, 0);
    expect_error(writer_source.finish(), ErrorCode::invalid_state, 0);
    REQUIRE(!moved_writer_context.finished() && !moved_writer_context.failed());
    REQUIRE(moved_writer_context.wire_bits() == 0);
    REQUIRE(moved_writer_context.logical_output_octets() == 0);
    REQUIRE(writer_destination.cursor_bit() == 0);

    const std::array<std::byte, 1> encoded{std::byte{0x80}};
    DecodeContext reader_context;
    auto made = BitReader::make(encoded, reader_context);
    REQUIRE(made);
    auto reader_source = std::move(made).value();
    BitReader reader_destination(std::move(reader_source));
    expect_error(reader_source.read_bit(), ErrorCode::invalid_state, 0);
    expect_error(reader_source.read_aligned_u16_be(), ErrorCode::invalid_state, 0);
    expect_error(reader_source.align_to_octet_zero(), ErrorCode::invalid_state, 0);
    expect_error(reader_source.validate_complete_value(), ErrorCode::invalid_state, 0);
    REQUIRE(!reader_context.finished() && !reader_context.failed());
    REQUIRE(reader_context.wire_bits() == 0);
    REQUIRE(reader_destination.cursor_bit() == 0);

    auto first = reader_destination.read_bit();
    REQUIRE(first && first.value());
    REQUIRE(reader_destination.validate_complete_value());
    const auto reader_wire = reader_context.wire_bits();
    const auto reader_cursor = reader_destination.cursor_bit();
    expect_error(reader_destination.read_bit(), ErrorCode::invalid_state, reader_cursor);
    expect_error(reader_destination.read_aligned_u16_be(), ErrorCode::invalid_state, reader_cursor);
    expect_error(reader_destination.align_to_octet_zero(), ErrorCode::invalid_state, reader_cursor);
    expect_error(reader_destination.validate_complete_value(), ErrorCode::invalid_state, reader_cursor);
    REQUIRE(reader_context.finished() && !reader_context.failed());
    REQUIRE(reader_context.wire_bits() == reader_wire);
    REQUIRE(reader_destination.cursor_bit() == reader_cursor);
}

void test_result_wrong_arm_accessors() {
    bool caught_value_error = false;
    try {
        (void)Result<int>::success(1).error();
    } catch(const std::bad_variant_access&) {
        caught_value_error = true;
    }
    REQUIRE(caught_value_error);

    bool caught_void_error = false;
    try {
        (void)Result<void>::success().error();
    } catch(const std::bad_variant_access&) {
        caught_void_error = true;
    }
    REQUIRE(caught_void_error);

    bool caught_value_access = false;
    try {
        (void)Result<int>::failure({ErrorCode::invalid_state, 3}).value();
    } catch(const std::bad_variant_access&) {
        caught_value_access = true;
    }
    REQUIRE(caught_value_access);
}

void test_checked_size_arithmetic() {
    std::size_t result = 0;
    const auto max = std::numeric_limits<std::size_t>::max();
    REQUIRE(!nrforge::aper::detail::checked_add_size(max, 1, result));
    REQUIRE(!nrforge::aper::detail::checked_add_size(max - 2, 3, result));
    REQUIRE(nrforge::aper::detail::checked_add_size(max - 1, 1, result));
    REQUIRE(result == max);
    REQUIRE(!nrforge::aper::detail::checked_octets_to_bits(max, result));
    REQUIRE(nrforge::aper::detail::checked_octets_to_bits(max / 8, result));
    REQUIRE(result == (max / 8) * 8);
    REQUIRE(nrforge::aper::detail::checked_bits_to_octets(max, result));
    REQUIRE(result == max / 8 + 1);
}

void test_invalid_logical_limit() {
    const std::array<std::byte, 1> byte{std::byte{0}};
    DecodeContext context;
    auto result = BitReader::make_bounded_for_test(byte, 9, context);
    expect_error(result, ErrorCode::invalid_argument, 0);
}

// Independent bit-list oracle for the four approved domains. It deliberately
// does not invoke runtime alignment, length or payload routines.
std::vector<std::byte> constrained_reference(unsigned width, std::uint64_t value,
                                            unsigned residue, unsigned forced_octets = 0) {
    std::vector<bool> bits(residue, true);
    const unsigned length_width = width > 16 ? (width == 32 ? 2u : 3u) : 0u;
    unsigned octets = width <= 16 ? width / 8 : 1;
    if(width > 16)
        while(octets < 8 && value >= (std::uint64_t{1} << (octets * 8))) ++octets;
    if(forced_octets) octets = forced_octets;
    for(unsigned i = length_width; i > 0; --i)
        bits.push_back(((octets - 1) & (1u << (i - 1))) != 0);
    while(bits.size() % 8) bits.push_back(false);
    for(unsigned i = octets * 8; i > 0; --i)
        bits.push_back((value & (std::uint64_t{1} << (i - 1))) != 0);
    std::vector<std::byte> bytes(bits.size() / 8, std::byte{0});
    for(std::size_t i = 0; i < bits.size(); ++i)
        if(bits[i]) bytes[i / 8] |= static_cast<std::byte>(0x80u >> (i % 8));
    return bytes;
}

void test_constrained_uint_vectors_and_failures() {
    for(unsigned width : {8u, 16u, 32u, 40u}) {
        const auto maximum = (std::uint64_t{1} << width) - 1;
        const std::uint64_t candidates[] = {0, 1, 254, 255, 256, 65535, 65536,
            16777215, 16777216, 4294967295ULL, 4294967296ULL, maximum};
        for(auto value : candidates) {
            if(value > maximum) continue;
            for(unsigned residue = 0; residue < 8; ++residue) {
                const auto bytes = constrained_reference(width, value, residue);
                const auto end = bytes.size() * 8;
                EncodeContext ec;
                BitWriter writer(ec);
                for(unsigned i = 0; i < residue; ++i) REQUIRE(writer.write_bit(true));
                FieldWriter field(writer);
                REQUIRE(field.write_constrained_uint(value, width));
                REQUIRE(writer.cursor_bit() == end && ec.wire_bits() == end);
                REQUIRE(ec.logical_output_octets() == bytes.size());
                auto encoded = writer.finish();
                REQUIRE(encoded && encoded.value().octets == bytes);
                REQUIRE(encoded.value().last_field_end_bit == end);
                REQUIRE(encoded.value().final_padding_bits == 0);
                REQUIRE(encoded.value().complete_encoding_bits == end);
                DecodeContext dc;
                auto made = BitReader::make(bytes, dc);
                REQUIRE(made);
                auto reader = std::move(made).value();
                for(unsigned i = 0; i < residue; ++i) REQUIRE(reader.read_bit().value());
                FieldReader input(reader);
                auto decoded = input.read_constrained_uint(width);
                REQUIRE(decoded && decoded.value() == value);
                REQUIRE(reader.cursor_bit() == end && dc.wire_bits() == end);
                REQUIRE(reader.validate_complete_value());

                // Every logical-bit truncation, including partial prefix and
                // alignment-complete / payload-incomplete cases, stays atomic.
                for(std::size_t limit = residue; limit < end; ++limit) {
                    DecodeContext tc;
                    auto bounded = BitReader::make_bounded_for_test(bytes, limit, tc);
                    REQUIRE(bounded);
                    auto truncated = std::move(bounded).value();
                    for(unsigned i = 0; i < residue; ++i) REQUIRE(truncated.read_bit());
                    expect_error(truncated.read_constrained_uint(width), ErrorCode::truncated_input, limit);
                    REQUIRE(truncated.cursor_bit() == residue && tc.wire_bits() == residue);
                    expect_error(truncated.read_constrained_uint(0), ErrorCode::truncated_input, limit);
                    expect_error(truncated.validate_complete_value(), ErrorCode::truncated_input, limit);
                }
                const auto prefix = width > 16 ? (width == 32 ? 2u : 3u) : 0u;
                const auto pad_start = residue + prefix;
                const auto payload_start = (pad_start + 7u) / 8u * 8u;
                for(unsigned position = pad_start; position < payload_start; ++position) {
                    auto bad = bytes;
                    bad[position / 8] |= static_cast<std::byte>(0x80u >> (position % 8));
                    DecodeContext pc;
                    auto result = BitReader::make(bad, pc);
                    REQUIRE(result);
                    auto padding_reader = std::move(result).value();
                    for(unsigned i = 0; i < residue; ++i) REQUIRE(padding_reader.read_bit());
                    expect_error(padding_reader.read_constrained_uint(width), ErrorCode::nonzero_padding, position);
                    REQUIRE(padding_reader.cursor_bit() == residue && pc.wire_bits() == residue);
                    expect_error(padding_reader.read_bit(), ErrorCode::nonzero_padding, position);
                }
                // Exact / one-less resource budgets in both directions.
                for(bool short_budget : {false, true}) {
                    for(bool output_budget : {false, true}) {
                        Limits limits;
                        if(output_budget) limits.max_output_octets = bytes.size() - (short_budget ? 1 : 0);
                        else limits.max_wire_bits = end - (short_budget ? 1 : 0);
                        EncodeContext bc(limits);
                        BitWriter bw(bc);
                        for(unsigned i = 0; i < residue; ++i) REQUIRE(bw.write_bit(true));
                        auto result = bw.write_constrained_uint(value, width);
                        if(short_budget) {
                            expect_error(result, ErrorCode::resource_limit, residue);
                            REQUIRE(bw.cursor_bit() == residue && bc.wire_bits() == residue);
                            REQUIRE(bc.logical_output_octets() == (residue ? 1u : 0u));
                            expect_error(bw.finish(), ErrorCode::resource_limit, residue);
                        } else {
                            REQUIRE(result);
                            REQUIRE(bw.finish().value().octets == bytes);
                        }
                    }
                    Limits limits;
                    limits.max_wire_bits = end - (short_budget ? 1 : 0);
                    DecodeContext bc(limits);
                    auto result = BitReader::make(bytes, bc);
                    REQUIRE(result);
                    auto br = std::move(result).value();
                    for(unsigned i = 0; i < residue; ++i) REQUIRE(br.read_bit());
                    auto read = br.read_constrained_uint(width);
                    if(short_budget) {
                        expect_error(read, ErrorCode::resource_limit, residue);
                        REQUIRE(br.cursor_bit() == residue && bc.wire_bits() == residue);
                    } else REQUIRE(read && read.value() == value);
                    limits = Limits{};
                    limits.max_input_octets = bytes.size() - (short_budget ? 1 : 0);
                    DecodeContext ic(limits);
                    auto input_result = BitReader::make(bytes, ic);
                    if(short_budget) expect_error(input_result, ErrorCode::resource_limit, 0);
                    else REQUIRE(input_result);
                }
            }
        }
        for(auto value : {maximum + 1, UINT64_MAX}) {
            EncodeContext c;
            BitWriter w(c);
            REQUIRE(w.write_bit(true));
            expect_error(w.write_constrained_uint(value, width), ErrorCode::constraint_violation, 1);
            REQUIRE(w.cursor_bit() == 1 && c.wire_bits() == 1 && c.logical_output_octets() == 1);
            expect_error(w.write_constrained_uint(0, 0), ErrorCode::constraint_violation, 1);
            expect_error(w.finish(), ErrorCode::constraint_violation, 1);
        }
    }
}

void test_constrained_uint_malformed_and_state() {
    for(unsigned width : {32u, 40u}) {
        // Illegal leading zero: semantic rejection after availability and budget.
        auto bytes = constrained_reference(width, 1, 3, 2);
        for(unsigned failure : {0u, 1u, 2u, 3u}) {
            Limits limits;
            if(failure == 1) limits.max_wire_bits = bytes.size() * 8 - 1;
            DecodeContext c(limits);
            if(failure == 3) bytes[0] |= std::byte{1}; // last alignment bit
            auto made = BitReader::make_bounded_for_test(bytes,
                bytes.size() * 8 - (failure == 2 ? 1 : 0), c);
            REQUIRE(made);
            auto r = std::move(made).value();
            for(unsigned i = 0; i < 3; ++i) REQUIRE(r.read_bit());
            const auto code = failure == 1 ? ErrorCode::resource_limit :
                (failure == 2 ? ErrorCode::truncated_input :
                 (failure == 3 ? ErrorCode::nonzero_padding : ErrorCode::constraint_violation));
            const auto offset = failure == 2 ? bytes.size() * 8 - 1 : (failure == 3 ? 7u : 3u);
            expect_error(r.read_constrained_uint(width), code, offset);
            REQUIRE(r.cursor_bit() == 3 && c.wire_bits() == 3);
        }
    }
    for(unsigned octets : {6u, 7u, 8u}) {
        const auto bytes = constrained_reference(40, 1, 3, octets);
        Limits limits;
        limits.max_wire_bits = 3; // selector priority over budget/payload absence
        DecodeContext c(limits);
        auto made = BitReader::make_bounded_for_test(bytes, 6, c);
        REQUIRE(made);
        auto r = std::move(made).value();
        for(unsigned i = 0; i < 3; ++i) REQUIRE(r.read_bit());
        expect_error(r.read_constrained_uint(40), ErrorCode::constraint_violation, 3);
        REQUIRE(r.cursor_bit() == 3 && c.wire_bits() == 3);
    }
    for(unsigned width : {0u, 1u, 7u, 9u, 24u, 64u, 65u, UINT_MAX}) {
        EncodeContext ec;
        BitWriter w(ec);
        expect_error(w.write_constrained_uint(UINT64_MAX, width), ErrorCode::invalid_argument, 0);
        expect_error(w.write_constrained_uint(0, 8), ErrorCode::invalid_argument, 0);
        REQUIRE(w.cursor_bit() == 0 && ec.wire_bits() == 0);
        DecodeContext dc;
        auto made = BitReader::make({}, dc);
        REQUIRE(made);
        auto r = std::move(made).value();
        expect_error(r.read_constrained_uint(width), ErrorCode::invalid_argument, 0);
        expect_error(r.read_constrained_uint(8), ErrorCode::invalid_argument, 0);
        REQUIRE(r.cursor_bit() == 0 && dc.wire_bits() == 0);
    }
    for(unsigned width : {0u, 8u, 16u, 32u, 40u}) {
        EncodeContext c;
        BitWriter w(c);
        REQUIRE(w.finish());
        expect_error(w.write_constrained_uint(UINT64_MAX, width), ErrorCode::invalid_state, 8);
        REQUIRE(c.finished() && !c.failed() && c.wire_bits() == 8);
        c.record_failure({ErrorCode::resource_limit, 42});
        expect_error(w.write_constrained_uint(0, width), ErrorCode::resource_limit, 42);
        EncodeContext mc;
        BitWriter source(mc);
        BitWriter destination(std::move(source));
        expect_error(source.write_constrained_uint(UINT64_MAX, width), ErrorCode::invalid_state, 0);
        REQUIRE(!mc.failed() && mc.wire_bits() == 0);
        REQUIRE(destination.write_constrained_uint(1, 8));
        const std::array<std::byte, 1> zero{std::byte{0}};
        DecodeContext dc;
        auto made = BitReader::make(zero, dc);
        REQUIRE(made);
        auto rs = std::move(made).value();
        BitReader rd(std::move(rs));
        expect_error(rs.read_constrained_uint(width), ErrorCode::invalid_state, 0);
        REQUIRE(!dc.failed() && dc.wire_bits() == 0);
        REQUIRE(rd.read_constrained_uint(8));
        REQUIRE(rd.validate_complete_value());
        expect_error(rd.read_constrained_uint(width), ErrorCode::invalid_state, 8);
        REQUIRE(dc.finished() && !dc.failed() && dc.wire_bits() == 8);
        dc.record_failure({ErrorCode::resource_limit, 42});
        expect_error(rd.read_constrained_uint(width), ErrorCode::resource_limit, 42);
    }
    for(unsigned width : {8u, 16u, 32u, 40u}) {
        for(bool prefix : {false, true}) {
            EncodeContext c;
            BitWriter w(c);
            if(prefix) REQUIRE(w.write_bit(true));
            fail_next_allocation = true;
            expect_error(w.write_constrained_uint((std::uint64_t{1} << width) - 1, width),
                         ErrorCode::allocation_failure, prefix ? 1 : 0);
            REQUIRE(!fail_next_allocation);
            REQUIRE(w.cursor_bit() == (prefix ? 1u : 0u));
            REQUIRE(c.wire_bits() == w.cursor_bit());
            REQUIRE(c.logical_output_octets() == (prefix ? 1u : 0u));
            expect_error(w.write_constrained_uint(0, 0), ErrorCode::allocation_failure, prefix ? 1 : 0);
        }
    }
    expect_error(nrforge::aper::encode_complete<int>(0, Limits{}, [](FieldWriter& f) {
        (void)f.write_constrained_uint(UINT64_MAX, 40);
        return f.write_constrained_uint(0, 8);
    }), ErrorCode::constraint_violation, 0);
    const std::array<std::byte, 1> incomplete{std::byte{0}};
    expect_error(nrforge::aper::decode_complete<std::uint64_t>(incomplete, Limits{}, [](FieldReader& f) {
        (void)f.read_constrained_uint(40);
        return f.read_constrained_uint(8);
    }), ErrorCode::truncated_input, 8);
}

} // namespace

void* operator new(std::size_t size) {
    if(fail_next_allocation) {
        fail_next_allocation = false;
        throw std::bad_alloc();
    }
    if(void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if(fail_next_allocation) {
        fail_next_allocation = false;
        throw std::bad_alloc();
    }
    if(void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    test_bit_order_and_boolean_selector();
    test_u16_values_and_alignment_residues();
    test_range_rejection_is_atomic_and_sticky();
    test_truncation_and_alignment_atomicity();
    test_nonzero_final_padding_trailing_and_empty();
    test_complete_wrappers_and_budgets();
    test_allocation_failure_atomicity();
    test_result_types_and_invalid_state();
    test_result_wrong_arm_accessors();
    test_checked_size_arithmetic();
    test_invalid_logical_limit();
    test_constrained_uint_vectors_and_failures();
    test_constrained_uint_malformed_and_state();
    return 0;
}
