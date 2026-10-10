#ifndef NRFORGE_APER_RUNTIME_HPP
#define NRFORGE_APER_RUNTIME_HPP

#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <type_traits>
#include <span>
#include <utility>
#include <variant>
#include <vector>
#include <string>
#include <string_view>

namespace nrforge::aper {

enum class CharacterStringKind { printable, visible, utf8 };

class FieldReader;
class FieldWriter;

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

// Independent root/addition wire indexes, not ASN.1 assigned numeric values.
// Unknown addition indexes are preserved through the full uint64 domain.
struct EnumeratedIndex {
    bool is_extension;
    std::uint64_t index;
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
    std::size_t max_extension_bitmap_bits = 1024;
    std::size_t max_retained_unknown_payload_octets = 1u << 20;
    std::size_t max_retained_unknown_records = 1024;
    std::size_t max_collection_elements = 65536;
    std::size_t max_known_open_staging_octets = 1u << 20;
    std::size_t max_known_open_depth = 16;
};

// A length segment precedes exactly count elements; a fragmented segment
// MUST be followed (after its payload) by another determinant, including zero.
struct CollectionLengthSegment { std::size_t count; bool fragmented; };

struct SequenceExtensionBitmap {
    std::size_t bit_count;
    std::vector<std::byte> packed_bits;
};

// Owned MSB-first BIT STRING storage: exactly ceil(bit_count/8) octets,
// with every unused low bit of the final storage octet zero.
struct BitString {
    std::vector<std::byte> octets;
    std::size_t bit_count = 0;
};

class DecodeContext {
public:
    explicit DecodeContext(Limits limits = {}) noexcept : limits_(limits) {}
    const Limits& limits() const noexcept { return limits_; }
    std::size_t wire_bits() const noexcept { return wire_bits_; }
    std::size_t collection_elements() const noexcept { return collection_elements_; }
    std::size_t known_open_staging_octets() const noexcept { return known_open_staging_octets_; }
    std::size_t known_open_depth() const noexcept { return known_open_depth_; }
    std::size_t extension_bitmap_bits() const noexcept { return extension_bitmap_bits_; }
    std::size_t retained_unknown_payload_octets() const noexcept { return retained_unknown_payload_octets_; }
    std::size_t retained_unknown_records() const noexcept { return retained_unknown_records_; }
    bool failed() const noexcept { return failed_; }
    bool finished() const noexcept { return finished_; }
    const Error* first_error() const noexcept { return failed_ ? &error_ : nullptr; }
    void record_failure(Error error) noexcept { fail(error); }

private:
    friend class BitReader;
    Limits limits_;
    std::size_t wire_bits_ = 0;
    std::size_t collection_elements_ = 0;
    std::size_t known_open_staging_octets_ = 0;
    std::size_t known_open_depth_ = 0;
    std::size_t extension_bitmap_bits_ = 0;
    std::size_t retained_unknown_payload_octets_ = 0;
    std::size_t retained_unknown_records_ = 0;
    const void* known_active_reader_ = nullptr;
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
    std::size_t collection_elements() const noexcept { return collection_elements_; }
    std::size_t known_open_staging_octets() const noexcept { return known_open_staging_octets_; }
    std::size_t known_open_depth() const noexcept { return known_open_depth_; }
    std::size_t logical_output_octets() const noexcept { return logical_output_octets_; }
    bool failed() const noexcept { return failed_; }
    bool finished() const noexcept { return finished_; }
    const Error* first_error() const noexcept { return failed_ ? &error_ : nullptr; }
    void record_failure(Error error) noexcept { fail(error); }

private:
    friend class BitWriter;
    Limits limits_;
    std::size_t wire_bits_ = 0;
    std::size_t collection_elements_ = 0;
    std::size_t known_open_staging_octets_ = 0;
    std::size_t known_open_depth_ = 0;
    std::size_t logical_output_octets_ = 0;
    const void* known_active_writer_ = nullptr;
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

// Canonical ascending, disjoint and non-adjacent finite permitted root intervals.
struct IntegerInterval { std::int64_t lower; std::int64_t upper; };

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
    // N16: one finite non-extensible INTEGER interval. Complete uint64/int64
    // domains are supported without computing an overflowing cardinality.
    Result<std::uint64_t> read_bounded_uint(std::uint64_t lower,std::uint64_t upper);
    Result<std::int64_t> read_bounded_int(std::int64_t lower,std::int64_t upper);
    // N19: bare extensible finite root; flag, alignment and signed payload atomic.
    Result<std::int64_t> read_extensible_int(std::int64_t lower,std::int64_t upper);
    Result<std::int64_t> read_integer_set(std::span<const IntegerInterval> root, bool extensible);
    // root_count 1..255; flags, length, alignment and index are atomic (N2).
    Result<EnumeratedIndex> read_enumerated(unsigned root_count, bool extensible);
    // N8: non-extensible SIZE 0<=lower<=upper<=65535; atomic count and charge.
    Result<CollectionLengthSegment> read_collection_segment();
    Result<std::size_t> read_bounded_collection_length(std::size_t lower, std::size_t upper);
    // N14: OCTET STRING; extensible=false retains the historical wire layout. Unconstrained mode supports lengths
    // through 16383; fragmented determinants are refused as resource_limit,
    // not a schema upper bound. Unconstrained mode requires lower=upper=0.
    // Batch SIZE: extensible=true adds a root/extension bit; outside-root
    // lengths use the unconstrained determinant and explicit 16383 ceiling.
    // Selector, alignment, determinant, payload and allocation are atomic.
    Result<std::vector<std::byte>> read_octet_string_owned(std::size_t lower,
        std::size_t upper, bool unconstrained = false, bool extensible = false);
    // Atomic APER OBJECT IDENTIFIER determinant, contents and canonical check.
    Result<std::vector<std::byte>> read_object_identifier_owned();
    // N15: length units are bits. Fixed <=16 is unaligned; all other
    // payloads align before their first bit, never after their last bit.
    // Unconstrained mode requires lower=upper=0 and refuses fragmentation.
    Result<BitString> read_bit_string_owned(std::size_t lower,
        std::size_t upper, bool unconstrained = false, bool extensible = false);
    // Printable/VisibleString: canonical ASCII repertoire, aligned 8-bit characters.
    // UTF8String: strict scalar validation; SIZE counts scalars but is not PER-visible.
    // Extensible SIZE is root/extension aware; lengths >=16384 are refused.
    // Unconstrained mode requires lower=upper=0, extensible=false; no selector bit.
    Result<std::string> read_character_string_owned(std::size_t lower,
        std::size_t upper, bool extensible = false, CharacterStringKind kind = CharacterStringKind::printable, bool unconstrained = false);
    // Opt-in bounded BIT SIZE with upper 65536..131072. Fragment units are bits.
    Result<BitString> read_bit_string_owned_fragmented_size(std::size_t lower, std::size_t upper);
    // N7-P2: owned, atomic extension framing; no inner payload interpretation.
    Result<SequenceExtensionBitmap> read_sequence_extension_bitmap();
    Result<std::vector<std::byte>> read_open_type_owned();
    template<class T, class DecodeFields>
    Result<T> read_known_open_type(DecodeFields&& decode_fields);
    Result<void> validate_complete_value();
    std::size_t cursor_bit() const noexcept { return cursor_bit_; }

private:
    friend class FieldReader;
    BitReader(std::span<const std::byte> input, DecodeContext& context,
              std::size_t logical_bit_limit) noexcept;
    Result<void> validate_live() const noexcept;
    Result<void> preflight(std::size_t bit_count, std::size_t start) noexcept;
    Result<void> fail(Error error) noexcept;
    Result<void> read_known_open_type_impl(void*, Result<void> (*)(FieldReader&, void*));
    Error map_known_error(Error) const noexcept;
    void charge_wire(std::size_t count) noexcept { if(!known_child_) context_->wire_bits_ += count; }
    bool known_child_ = false;
    bool locally_finished_ = false;
    const BitReader* known_origin_ = nullptr;
    std::size_t known_frame_start_ = 0;
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
    Result<void> write_bounded_uint(std::uint64_t value,std::uint64_t lower,std::uint64_t upper);
    Result<void> write_bounded_int(std::int64_t value,std::int64_t lower,std::int64_t upper);
    Result<void> write_extensible_int(std::int64_t value,std::int64_t lower,std::int64_t upper);
    Result<void> write_integer_set(std::int64_t value,std::span<const IntegerInterval> root, bool extensible);
    // root_count 1..255; extension indexes require extensible=true (N2).
    Result<void> write_enumerated(EnumeratedIndex value, unsigned root_count, bool extensible);
    Result<void> write_collection_segment(std::size_t count, bool fragmented);
    Result<void> write_bounded_collection_length(std::uint64_t count, std::size_t lower, std::size_t upper);
    Result<void> write_octet_string(std::span<const std::byte> value,
        std::size_t lower, std::size_t upper, bool unconstrained = false, bool extensible = false);
    Result<void> write_bit_string(const BitString& value,
        std::size_t lower, std::size_t upper, bool unconstrained = false, bool extensible = false);
    Result<void> write_character_string(std::string_view value,
        std::size_t lower, std::size_t upper, bool extensible = false, CharacterStringKind kind = CharacterStringKind::printable, bool unconstrained = false);
    Result<void> write_bit_string_fragmented_size(const BitString& value, std::size_t lower, std::size_t upper);
    template<class EncodeFields>
    Result<void> write_known_open_type(EncodeFields&& encode_fields);
    Result<void> reject_sequence_extension_data();
    Result<CompleteEncoding> finish();
    std::size_t cursor_bit() const noexcept { return cursor_bit_; }

private:
    friend class FieldWriter;
    Result<void> validate_live() const noexcept;
    Result<void> preflight(std::size_t bit_count, std::size_t projected_end,
                           std::size_t start) noexcept;
    Result<void> grow_to(std::size_t octets, std::size_t start) noexcept;
    Result<void> fail(Error error) noexcept;
    Result<void> write_known_open_type_impl(void*, Result<void> (*)(FieldWriter&, void*));
    void charge_wire(std::size_t count) noexcept { if(!known_child_) context_->wire_bits_ += count; }
    void publish_output(std::size_t count) noexcept { if(!known_child_) context_->logical_output_octets_ = count; }
    bool known_child_ = false;
    bool locally_finished_ = false;
    std::size_t known_encode_anchor_ = 0;
    std::size_t known_staged_octets_ = 0;
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
    Result<std::uint64_t> read_bounded_uint(std::uint64_t lower,std::uint64_t upper) {
        return reader_.read_bounded_uint(lower,upper);
    }
    Result<std::int64_t> read_integer_set(std::span<const IntegerInterval> root,bool extensible) {
        return reader_.read_integer_set(root,extensible);
    }
    Result<std::int64_t> read_extensible_int(std::int64_t lower,std::int64_t upper) {
        return reader_.read_extensible_int(lower,upper);
    }
    Result<std::int64_t> read_bounded_int(std::int64_t lower,std::int64_t upper) {
        return reader_.read_bounded_int(lower,upper);
    }
    Result<EnumeratedIndex> read_enumerated(unsigned root_count, bool extensible) {
        return reader_.read_enumerated(root_count, extensible);
    }
    Result<CollectionLengthSegment> read_collection_segment() { return reader_.read_collection_segment(); }
    Result<std::size_t> read_bounded_collection_length(std::size_t lower, std::size_t upper) {
        return reader_.read_bounded_collection_length(lower, upper);
    }
    Result<std::vector<std::byte>> read_octet_string_owned(std::size_t lower,
        std::size_t upper, bool unconstrained = false, bool extensible = false) {
        return reader_.read_octet_string_owned(lower, upper, unconstrained, extensible);
    }
    Result<BitString> read_bit_string_owned(std::size_t lower,
        std::size_t upper, bool unconstrained = false, bool extensible = false) {
        return reader_.read_bit_string_owned(lower, upper, unconstrained, extensible);
    }
    Result<BitString> read_bit_string_owned_fragmented_size(std::size_t lower, std::size_t upper) {
        return reader_.read_bit_string_owned_fragmented_size(lower, upper);
    }
    // Generated helper failures preserve sticky state and lifecycle semantics.
    Result<void> record_failure(Error error) {
        auto live = reader_.validate_live();
        return live ? reader_.fail(error) : live;
    }
    Result<std::string> read_character_string_owned(std::size_t lower, std::size_t upper, bool extensible = false, CharacterStringKind kind = CharacterStringKind::printable, bool unconstrained = false) {
        return reader_.read_character_string_owned(lower, upper, extensible, kind, unconstrained);
    }
    Result<SequenceExtensionBitmap> read_sequence_extension_bitmap() {
        return reader_.read_sequence_extension_bitmap();
    }
    Result<std::vector<std::byte>> read_object_identifier_owned() {
        return reader_.read_object_identifier_owned();
    }
    Result<std::vector<std::byte>> read_open_type_owned() { return reader_.read_open_type_owned(); }
    template<class T, class DecodeFields>
    Result<T> read_known_open_type(DecodeFields&& decode_fields) {
        return reader_.template read_known_open_type<T>(std::forward<DecodeFields>(decode_fields));
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
    Result<void> write_bounded_uint(std::uint64_t value,std::uint64_t lower,std::uint64_t upper) {
        return writer_.write_bounded_uint(value,lower,upper);
    }
    Result<void> write_integer_set(std::int64_t value,std::span<const IntegerInterval> root,bool extensible) {
        return writer_.write_integer_set(value,root,extensible);
    }
    Result<void> write_extensible_int(std::int64_t value,std::int64_t lower,std::int64_t upper) {
        return writer_.write_extensible_int(value,lower,upper);
    }
    Result<void> write_bounded_int(std::int64_t value,std::int64_t lower,std::int64_t upper) {
        return writer_.write_bounded_int(value,lower,upper);
    }
    Result<void> write_enumerated(EnumeratedIndex value, unsigned root_count, bool extensible) {
        return writer_.write_enumerated(value, root_count, extensible);
    }
    Result<void> write_collection_segment(std::size_t count, bool fragmented) { return writer_.write_collection_segment(count, fragmented); }
    Result<void> write_bounded_collection_length(std::uint64_t count, std::size_t lower, std::size_t upper) {
        return writer_.write_bounded_collection_length(count, lower, upper);
    }
    Result<void> write_octet_string(std::span<const std::byte> value,
        std::size_t lower, std::size_t upper, bool unconstrained = false, bool extensible = false) {
        return writer_.write_octet_string(value, lower, upper, unconstrained, extensible);
    }
    Result<void> write_bit_string(const BitString& value,
        std::size_t lower, std::size_t upper, bool unconstrained = false, bool extensible = false) {
        return writer_.write_bit_string(value, lower, upper, unconstrained, extensible);
    }
    Result<void> write_bit_string_fragmented_size(const BitString& value, std::size_t lower, std::size_t upper) {
        return writer_.write_bit_string_fragmented_size(value, lower, upper);
    }
    Result<void> reject_sequence_extension_data() { return writer_.reject_sequence_extension_data(); }
    Result<void> write_character_string(std::string_view value, std::size_t lower, std::size_t upper, bool extensible = false, CharacterStringKind kind = CharacterStringKind::printable, bool unconstrained = false) {
        return writer_.write_character_string(value, lower, upper, extensible, kind, unconstrained);
    }
    template<class EncodeFields>
    Result<void> write_known_open_type(EncodeFields&& encode_fields) {
        return writer_.write_known_open_type(std::forward<EncodeFields>(encode_fields));
    }
    Result<void> record_failure(Error error) {
        auto live = writer_.validate_live();
        return live ? writer_.fail(error) : live;
    }
    std::size_t cursor_bit() const noexcept { return writer_.cursor_bit(); }
private:
    BitWriter& writer_;
};

/* OBJECT IDENTIFIER values retain canonical BER content octets, allowing arcs
 * larger than machine integers. The APER length budget remains shared. */
struct ObjectIdentifierMapping {
    using value_type = std::vector<std::byte>;
    static constexpr bool canonical_ber_content = true;
};
struct OpaqueOpenTypeMapping { using value_type = std::vector<std::byte>; };
Result<void> write_private_open_payload(FieldWriter&, std::span<const std::byte>);
Result<std::vector<std::byte>> read_private_open_payload(FieldReader&);
bool object_identifier_content_valid(std::span<const std::byte>) noexcept;
Result<void> write_object_identifier(FieldWriter&, std::span<const std::byte>);
Result<std::vector<std::byte>> read_object_identifier(FieldReader&);

template<class T, class DecodeFields>
Result<T> BitReader::read_known_open_type(DecodeFields&& decode_fields) {
    static_assert(std::is_nothrow_move_constructible_v<T>, "known-open values require nothrow move publication");
    struct State {
        std::remove_reference_t<DecodeFields>* callback;
        std::optional<Result<T>> value;
    } state{&decode_fields, {}};
    auto status = read_known_open_type_impl(&state, +[](FieldReader& child, void* pointer) {
        auto& stored = *static_cast<State*>(pointer);
        auto decoded = std::forward<DecodeFields>(*stored.callback)(child);
        if(!decoded) return Result<void>::failure(decoded.error());
        stored.value.emplace(std::move(decoded));
        return Result<void>::success();
    });
    if(!status) return Result<T>::failure(status.error());
    return std::move(*state.value);
}

template<class EncodeFields>
Result<void> BitWriter::write_known_open_type(EncodeFields&& encode_fields) {
    struct State { std::remove_reference_t<EncodeFields>* callback; } state{&encode_fields};
    return write_known_open_type_impl(&state, +[](FieldWriter& child, void* pointer) {
        return std::forward<EncodeFields>(*static_cast<State*>(pointer)->callback)(child);
    });
}

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
