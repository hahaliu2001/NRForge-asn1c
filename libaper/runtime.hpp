#ifndef NRFORGE_APER_RUNTIME_HPP
#define NRFORGE_APER_RUNTIME_HPP

#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace nrforge::aper {

namespace detail {
bool checked_add_size(std::size_t left, std::size_t right, std::size_t& out) noexcept;
bool checked_bits_to_octets(std::size_t bits, std::size_t& octets) noexcept;
bool checked_octets_to_bits(std::size_t octets, std::size_t& bits) noexcept;
} // namespace detail

enum class ErrorCode {
    invalid_argument,
    constraint_violation,
    truncated_input,
    nonzero_padding,
    trailing_data,
    resource_limit,
    allocation_failure,
    invalid_state,
};

struct Error {
    ErrorCode code;
    std::size_t bit_offset;
};

template<class T>
class [[nodiscard]] Result {
public:
    // Accessing value() on failure or error() on success throws
    // std::bad_variant_access, consistently with Result<void>.
    static Result success(T value) { return Result(std::move(value)); }
    static Result failure(Error error) { return Result(error); }

    bool has_value() const noexcept { return std::holds_alternative<Value>(data_); }
    explicit operator bool() const noexcept { return has_value(); }
    T& value() & { return std::get<Value>(data_).value; }
    const T& value() const & { return std::get<Value>(data_).value; }
    T&& value() && { return std::get<Value>(std::move(data_)).value; }
    const Error& error() const & { return std::get<Error>(data_); }

private:
    struct Value { T value; };
    explicit Result(T value) : data_(std::in_place_type<Value>, Value{std::move(value)}) {}
    explicit Result(Error error) : data_(std::in_place_type<Error>, error) {}
    std::variant<Value, Error> data_;
};

template<>
class [[nodiscard]] Result<void> {
public:
    // Accessing error() on success throws std::bad_variant_access, as in Result<T>.
    static Result success() noexcept { return Result(std::monostate{}); }
    static Result failure(Error error) noexcept { return Result(error); }
    bool has_value() const noexcept { return std::holds_alternative<std::monostate>(data_); }
    explicit operator bool() const noexcept { return has_value(); }
    const Error& error() const & { return std::get<Error>(data_); }

private:
    explicit Result(std::monostate) noexcept : data_(std::monostate{}) {}
    explicit Result(Error error) noexcept : data_(error) {}
    std::variant<std::monostate, Error> data_;
};

struct Limits {
    std::size_t max_input_octets = 1u << 20;
    std::size_t max_output_octets = 1u << 20;
    std::size_t max_wire_bits = 8u << 20;
};

class DecodeContext {
public:
    explicit DecodeContext(Limits limits = {}) noexcept : limits_(limits) {}
    const Limits& limits() const noexcept { return limits_; }
    std::size_t wire_bits() const noexcept { return wire_bits_; }
    bool failed() const noexcept { return failed_; }
    bool finished() const noexcept { return finished_; }
    const Error* first_error() const noexcept { return failed_ ? &error_ : nullptr; }
    void record_failure(Error error) noexcept { fail(error); }

private:
    friend class BitReader;
    Limits limits_;
    std::size_t wire_bits_ = 0;
    bool failed_ = false;
    bool finished_ = false;
    Error error_{ErrorCode::invalid_state, 0};
    void fail(Error error) noexcept { if(!failed_) { failed_ = true; error_ = error; } }
};

class EncodeContext {
public:
    explicit EncodeContext(Limits limits = {}) noexcept : limits_(limits) {}
    const Limits& limits() const noexcept { return limits_; }
    std::size_t wire_bits() const noexcept { return wire_bits_; }
    std::size_t logical_output_octets() const noexcept { return logical_output_octets_; }
    bool failed() const noexcept { return failed_; }
    bool finished() const noexcept { return finished_; }
    const Error* first_error() const noexcept { return failed_ ? &error_ : nullptr; }
    void record_failure(Error error) noexcept { fail(error); }

private:
    friend class BitWriter;
    Limits limits_;
    std::size_t wire_bits_ = 0;
    std::size_t logical_output_octets_ = 0;
    bool failed_ = false;
    bool finished_ = false;
    Error error_{ErrorCode::invalid_state, 0};
    void fail(Error error) noexcept { if(!failed_) { failed_ = true; error_ = error; } }
};

struct CompleteEncoding {
    std::vector<std::byte> octets;
    std::size_t last_field_end_bit = 0;
    std::uint8_t final_padding_bits = 0;
    bool empty_encoding_substitution = false;
    std::size_t complete_encoding_bits = 0;
    std::size_t octet_count = 0;
};

class BitReader {
public:
    static Result<BitReader> make(std::span<const std::byte> input, DecodeContext& context);
    static Result<BitReader> make_bounded_for_test(std::span<const std::byte> input,
                                                   std::size_t logical_bit_limit,
                                                   DecodeContext& context);
    BitReader(const BitReader&) = delete;
    BitReader& operator=(const BitReader&) = delete;
    BitReader(BitReader&& other) noexcept;
    BitReader& operator=(BitReader&&) = delete;

    Result<bool> read_bit();
    Result<void> align_to_octet_zero();
    Result<std::uint16_t> read_aligned_u16_be();
    // Zero-based, non-extensible INTEGER domains with root_bits 8/16/32/40.
    // Prefix, octet alignment and payload form one atomic operation (N1).
    Result<std::uint64_t> read_constrained_uint(unsigned root_bits);
    Result<void> validate_complete_value();
    std::size_t cursor_bit() const noexcept { return cursor_bit_; }

private:
    BitReader(std::span<const std::byte> input, DecodeContext& context,
              std::size_t logical_bit_limit) noexcept;
    Result<void> validate_live() const noexcept;
    Result<void> preflight(std::size_t bit_count, std::size_t start) noexcept;
    Result<void> fail(Error error) noexcept;
    std::span<const std::byte> input_;
    DecodeContext* context_;
    std::size_t logical_bit_limit_;
    std::size_t cursor_bit_ = 0;
};

class BitWriter {
public:
    explicit BitWriter(EncodeContext& context) noexcept : context_(&context) {}
    BitWriter(const BitWriter&) = delete;
    BitWriter& operator=(const BitWriter&) = delete;
    BitWriter(BitWriter&& other) noexcept;
    BitWriter& operator=(BitWriter&&) = delete;

    Result<void> write_bit(bool value);
    Result<void> align_to_octet_zero();
    Result<void> write_aligned_u16_be(std::uint64_t value);
    Result<void> write_constrained_uint(std::uint64_t value, unsigned root_bits);
    Result<CompleteEncoding> finish();
    std::size_t cursor_bit() const noexcept { return cursor_bit_; }

private:
    Result<void> validate_live() const noexcept;
    Result<void> preflight(std::size_t bit_count, std::size_t projected_end,
                           std::size_t start) noexcept;
    Result<void> grow_to(std::size_t octets, std::size_t start) noexcept;
    Result<void> fail(Error error) noexcept;
    EncodeContext* context_;
    std::vector<std::byte> output_;
    std::size_t cursor_bit_ = 0;
};

class FieldReader {
public:
    explicit FieldReader(BitReader& reader) noexcept : reader_(reader) {}
    FieldReader(const FieldReader&) = delete;
    FieldReader& operator=(const FieldReader&) = delete;
    Result<bool> read_bit() { return reader_.read_bit(); }
    Result<void> align_to_octet_zero() { return reader_.align_to_octet_zero(); }
    Result<std::uint16_t> read_aligned_u16_be() { return reader_.read_aligned_u16_be(); }
    Result<std::uint64_t> read_constrained_uint(unsigned root_bits) {
        return reader_.read_constrained_uint(root_bits);
    }
    std::size_t cursor_bit() const noexcept { return reader_.cursor_bit(); }
private:
    BitReader& reader_;
};

class FieldWriter {
public:
    explicit FieldWriter(BitWriter& writer) noexcept : writer_(writer) {}
    FieldWriter(const FieldWriter&) = delete;
    FieldWriter& operator=(const FieldWriter&) = delete;
    Result<void> write_bit(bool value) { return writer_.write_bit(value); }
    Result<void> align_to_octet_zero() { return writer_.align_to_octet_zero(); }
    Result<void> write_aligned_u16_be(std::uint64_t value) {
        return writer_.write_aligned_u16_be(value);
    }
    Result<void> write_constrained_uint(std::uint64_t value, unsigned root_bits) {
        return writer_.write_constrained_uint(value, root_bits);
    }
    std::size_t cursor_bit() const noexcept { return writer_.cursor_bit(); }
private:
    BitWriter& writer_;
};

template<class T, class DecodeFields>
Result<T> decode_complete(std::span<const std::byte> input, const Limits& limits,
                          DecodeFields&& decode_fields) {
    DecodeContext context(limits);
    auto made = BitReader::make(input, context);
    if(!made) return Result<T>::failure(made.error());
    auto reader = std::move(made).value();
    FieldReader fields(reader);
    try {
        auto decoded = std::forward<DecodeFields>(decode_fields)(fields);
        if(context.failed()) return Result<T>::failure(*context.first_error());
        if(!decoded) {
            context.record_failure(decoded.error());
            return Result<T>::failure(*context.first_error());
        }
        auto complete = reader.validate_complete_value();
        if(!complete) return Result<T>::failure(complete.error());
        if(context.failed()) return Result<T>::failure(*context.first_error());
        return Result<T>::success(std::move(decoded).value());
    } catch(const std::bad_alloc&) {
        Error error{ErrorCode::allocation_failure, reader.cursor_bit()};
        context.record_failure(error);
        return Result<T>::failure(*context.first_error());
    }
}

template<class T, class EncodeFields>
Result<CompleteEncoding> encode_complete(const T& value, const Limits& limits,
                                         EncodeFields&& encode_fields) {
    (void)value;
    EncodeContext context(limits);
    BitWriter writer(context);
    FieldWriter fields(writer);
    try {
        auto encoded = std::forward<EncodeFields>(encode_fields)(fields);
        if(context.failed()) return Result<CompleteEncoding>::failure(*context.first_error());
        if(!encoded) {
            context.record_failure(encoded.error());
            return Result<CompleteEncoding>::failure(*context.first_error());
        }
        auto complete = writer.finish();
        if(!complete) return Result<CompleteEncoding>::failure(complete.error());
        if(context.failed()) return Result<CompleteEncoding>::failure(*context.first_error());
        return complete;
    } catch(const std::bad_alloc&) {
        Error error{ErrorCode::allocation_failure, writer.cursor_bit()};
        context.record_failure(error);
        return Result<CompleteEncoding>::failure(*context.first_error());
    }
}

} // namespace nrforge::aper

#endif
