#ifndef NRFORGE_UPER_RUNTIME_HPP
#define NRFORGE_UPER_RUNTIME_HPP
#include <bit>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace nrforge::uper {
enum class ErrorCode {
    invalid_argument,
    constraint_violation,
    truncated_input,
    nonzero_padding,
    trailing_data,
    resource_limit,
    allocation_failure,
    invalid_state
};
struct Error {
    ErrorCode code;
    std::size_t bit_offset;
};
template <class T> class [[nodiscard]] Result {
    struct Value {
        T value;
    };
    std::variant<Value, Error> data_;
    explicit Result(T v) : data_(std::in_place_type<Value>, Value{std::move(v)}) {}
    explicit Result(Error e) : data_(e) {}

  public:
    static Result success(T v) { return Result(std::move(v)); }
    static Result failure(Error e) { return Result(e); }
    explicit operator bool() const noexcept { return std::holds_alternative<Value>(data_); }
    T &value() & { return std::get<Value>(data_).value; }
    const T &value() const & { return std::get<Value>(data_).value; }
    T &&value() && { return std::get<Value>(std::move(data_)).value; }
    const Error &error() const { return std::get<Error>(data_); }
};
template <> class [[nodiscard]] Result<void> {
    std::optional<Error> error_;
    explicit Result(std::optional<Error> e) : error_(e) {}

  public:
    static Result success() { return Result({}); }
    static Result failure(Error e) { return Result(e); }
    explicit operator bool() const noexcept { return !error_; }
    const Error &error() const {
        if (!error_)
            throw std::bad_variant_access();
        return *error_;
    }
};
struct Limits {
    std::size_t max_input_octets = 1u << 20, max_output_octets = 1u << 20;
    std::size_t max_wire_bits = 8u << 20, max_value_octets = 1u << 20;
    std::size_t max_collection_elements = 65536, max_bitmap_bits = 1024;
    std::size_t max_unknown_octets = 1u << 20, max_unknown_records = 1024;
    std::size_t max_depth = 64, max_work_bits = 16u << 20, max_open_staging_octets = 1u << 20;
};
struct Usage {
    std::size_t value_octets = 0, collection_elements = 0, bitmap_bits = 0, unknown_octets = 0,
                unknown_records = 0, work_bits = 0, open_staging_octets = 0;
};
struct Size {
    std::size_t lower = 0, upper = 0;
    bool unconstrained = false, extensible = false;
};
struct Index {
    bool extension = false;
    std::uint64_t index = 0;
    bool operator==(const Index &) const = default;
};
struct LengthSegment {
    std::size_t count;
    bool fragmented;
};
struct BitString {
    std::vector<std::byte> octets;
    std::size_t bit_count = 0;
    bool operator==(const BitString &) const = default;
};
// Exact is a field-list boundary, not complete ASN.1 bit-string containment.
// Contained_bits substitutes one zero bit for an empty encoding (11.1.3.2).
enum class Completion { octets, contained_bits, exact };
struct CompleteEncoding {
    std::vector<std::byte> octets;
    std::size_t semantic_bits = 0, encoded_bits = 0;
    unsigned padding_bits = 0;
    bool empty_substitution = false;
};
namespace detail {
struct Fault {
    Error error;
};
std::size_t octets_for_bits(std::size_t) noexcept;
} // namespace detail

// One stream owns its sticky error and cumulative budgets. Each public field
// operation restores cursor, quotas and staging on failure. No alignment API.
// Input is borrowed for reader lifetime; all returned payloads are owned.
class Reader {
    std::span<const std::byte> input_;
    Limits limits_;
    Usage usage_;
    std::size_t cursor_ = 0, limit_ = 0;
    std::optional<Error> error_;
    bool finished_ = false;
    std::size_t depth_ = 0, base_depth_ = 0;
    Reader(std::span<const std::byte>, Limits, std::size_t) noexcept;
    [[noreturn]] void fault(ErrorCode) const;
    void live() const;
    std::uint64_t bits(unsigned);
    std::uint64_t bounded(std::uint64_t, std::uint64_t);
    LengthSegment length();
    std::uint64_t small();
    std::uint64_t integer(bool);
    BitString blob(Size, bool, bool retain = false);
    void charge(std::size_t &, std::size_t, std::size_t);
    template <class T, class F> Result<T> transaction(F &&f) {
        auto start = cursor_;
        auto usage = usage_;
        auto depth = depth_;
        try {
            live();
            return Result<T>::success(f());
        } catch (const detail::Fault &e) {
            cursor_ = start;
            usage_ = usage;
            depth_ = depth;
            if (!error_)
                error_ = e.error;
            return Result<T>::failure(*error_);
        } catch (const std::bad_alloc &) {
            cursor_ = start;
            usage_ = usage;
            depth_ = depth;
            if (!error_)
                error_ = Error{ErrorCode::allocation_failure, start};
            return Result<T>::failure(*error_);
        } catch (const std::length_error &) {
            cursor_ = start;
            usage_ = usage;
            depth_ = depth;
            if (!error_)
                error_ = Error{ErrorCode::resource_limit, start};
            return Result<T>::failure(*error_);
        } catch (...) {
            cursor_ = start;
            usage_ = usage;
            depth_ = depth;
            throw;
        }
    }

  public:
    static Result<Reader> make(std::span<const std::byte>, Limits = {},
                               std::optional<std::size_t> logical_bits = {});
    Reader(const Reader &) = delete;
    Reader &operator=(const Reader &) = delete;
    Reader(Reader &&) noexcept;
    Reader &operator=(Reader &&) = delete;
    Result<std::uint64_t> read_bits(unsigned);
    Result<std::uint64_t> read_uint(std::uint64_t lower, std::uint64_t upper);
    Result<std::int64_t> read_int(std::int64_t lower, std::int64_t upper);
    Result<std::int64_t> read_unconstrained_int();
    Result<std::uint64_t> read_extensible_uint(std::uint64_t lower, std::uint64_t upper);
    Result<std::int64_t> read_extensible_int(std::int64_t lower, std::int64_t upper);
    Result<std::uint64_t> read_normally_small();
    Result<Index> read_index(std::uint64_t root_count, bool extensible);
    Result<LengthSegment> read_collection_segment();
    Result<void> claim_value_octets(std::size_t);
    Result<std::size_t> read_collection_length(std::size_t lower, std::size_t upper);
    Result<std::vector<std::byte>> read_octets(Size);
    Result<BitString> read_bit_string(Size);
    // At most 16383 bitmap bits in this runtime profile. Bits are packed MSB first.
    Result<BitString> read_extension_bitmap();
    Result<std::vector<std::byte>> read_open_type(bool retain_unknown = false);
    Result<void> skip_open_type();
    // Callback operates on this borrowed stream; it must not move or finish it.
    // Complete compounds restore all fields/quotas on any nested failure.
    template <class T, class F> Result<T> compound(F &&f) {
        return transaction<T>([&] {
            if (depth_ >= limits_.max_depth)
                fault(ErrorCode::resource_limit);
            ++depth_;
            auto result = f(*this);
            if (error_)
                throw detail::Fault{*error_};
            --depth_;
            if (!result)
                throw detail::Fault{result.error()};
            return std::move(result).value();
        });
    }
    template <class T, class F> Result<T> read_known_open(F &&f) {
        return transaction<T>([&] {
            auto anchor = cursor_;
            if (depth_ >= limits_.max_depth)
                fault(ErrorCode::resource_limit);
            auto payload = read_open_type();
            if (!payload)
                throw detail::Fault{payload.error()};
            auto made = make(payload.value(), limits_);
            if (!made)
                throw detail::Fault{{made.error().code, anchor}};
            auto child = std::move(made).value();
            child.usage_ = usage_;
            child.depth_ = child.base_depth_ = depth_ + 1;
            auto result = [&] {
                try {
                    return f(child);
                } catch (const detail::Fault &e) {
                    throw detail::Fault{{e.error.code, anchor}};
                }
            }();
            if (!result)
                throw detail::Fault{{result.error().code, anchor}};
            auto end = child.finish();
            if (!end)
                throw detail::Fault{{end.error().code, anchor}};
            usage_ = child.usage_;
            return std::move(result).value();
        });
    }
    Result<void> finish(Completion = Completion::octets);
    std::size_t cursor_bit() const noexcept { return cursor_; }
    const Usage &usage() const noexcept { return usage_; }
    const Error *first_error() const noexcept { return error_ ? &*error_ : nullptr; }
};
class Writer {
    Limits limits_;
    Usage usage_;
    std::vector<std::byte> output_;
    std::size_t cursor_ = 0;
    std::optional<Error> error_;
    bool finished_ = false, child_ = false;
    std::size_t depth_ = 0, base_depth_ = 0;
    [[noreturn]] void fault(ErrorCode) const;
    void live() const;
    void bits(std::uint64_t, unsigned);
    void bounded(std::uint64_t, std::uint64_t, std::uint64_t);
    void length(std::size_t, bool);
    void small(std::uint64_t);
    void integer(std::uint64_t, bool);
    void reject_alias(std::span<const std::byte>) const;
    void blob(std::span<const std::byte>, std::size_t, Size, bool);
    void charge(std::size_t &, std::size_t, std::size_t);
    template <class F> Result<void> transaction(F &&f) {
        auto start = cursor_;
        auto size = output_.size();
        auto usage = usage_;
        auto depth = depth_;
        auto tail = size ? output_.back() : std::byte{0};
        try {
            live();
            f();
            return Result<void>::success();
        } catch (const detail::Fault &e) {
            cursor_ = start;
            usage_ = usage;
            depth_ = depth;
            output_.resize(size);
            if (size)
                output_.back() = tail;
            if (!error_)
                error_ = e.error;
            return Result<void>::failure(*error_);
        } catch (const std::bad_alloc &) {
            cursor_ = start;
            usage_ = usage;
            depth_ = depth;
            output_.resize(size);
            if (size)
                output_.back() = tail;
            if (!error_)
                error_ = Error{ErrorCode::allocation_failure, start};
            return Result<void>::failure(*error_);
        } catch (const std::length_error &) {
            cursor_ = start;
            usage_ = usage;
            depth_ = depth;
            output_.resize(size);
            if (size)
                output_.back() = tail;
            if (!error_)
                error_ = Error{ErrorCode::resource_limit, start};
            return Result<void>::failure(*error_);
        } catch (...) {
            cursor_ = start;
            usage_ = usage;
            depth_ = depth;
            output_.resize(size);
            if (size)
                output_.back() = tail;
            throw;
        }
    }

  public:
    explicit Writer(Limits limits = {}) noexcept : limits_(limits) {}
    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;
    Writer(Writer &&) = delete;
    Writer &operator=(Writer &&) = delete;
    Result<void> write_bits(std::uint64_t, unsigned);
    Result<void> write_uint(std::uint64_t value, std::uint64_t lower, std::uint64_t upper);
    Result<void> write_int(std::int64_t value, std::int64_t lower, std::int64_t upper);
    Result<void> write_unconstrained_int(std::int64_t);
    Result<void> write_extensible_uint(std::uint64_t value, std::uint64_t lower,
                                       std::uint64_t upper);
    Result<void> write_extensible_int(std::int64_t value, std::int64_t lower, std::int64_t upper);
    Result<void> write_normally_small(std::uint64_t);
    Result<void> write_index(Index, std::uint64_t root_count, bool extensible);
    Result<void> write_collection_segment(std::size_t, bool fragmented);
    Result<void> write_collection_length(std::size_t, std::size_t lower, std::size_t upper);
    Result<void> write_octets(std::span<const std::byte>, Size);
    Result<void> write_bit_string(const BitString &, Size);
    Result<void> write_extension_bitmap(const BitString &);
    Result<void> write_extension_bitmap(std::span<const std::byte>, std::size_t);
    // Payload must already be a nonempty complete octet encoding, including
    // inner terminal padding/substitution. This primitive does not decode it.
    Result<void> write_open_type(std::span<const std::byte>);
    template <class F> Result<void> compound(F &&f) {
        return transaction([&] {
            if (depth_ >= limits_.max_depth)
                fault(ErrorCode::resource_limit);
            ++depth_;
            auto result = f(*this);
            if (error_)
                throw detail::Fault{*error_};
            --depth_;
            if (!result)
                throw detail::Fault{result.error()};
        });
    }
    template <class F> Result<void> write_known_open(F &&f) {
        return transaction([&] {
            auto anchor = cursor_;
            if (depth_ >= limits_.max_depth)
                fault(ErrorCode::resource_limit);
            Writer child(limits_);
            child.child_ = true;
            child.usage_ = usage_;
            child.depth_ = child.base_depth_ = depth_ + 1;
            auto status = [&] {
                try {
                    return f(child);
                } catch (const detail::Fault &e) {
                    throw detail::Fault{{e.error.code, anchor}};
                }
            }();
            if (!status)
                throw detail::Fault{{status.error().code, anchor}};
            auto encoding = child.finish();
            if (!encoding)
                throw detail::Fault{{encoding.error().code, anchor}};
            usage_ = child.usage_;
            auto &payload = encoding.value().octets;
            blob(payload, payload.size(), Size{0, 0, true}, true);
            usage_.open_staging_octets -= payload.size();
        });
    }
    Result<CompleteEncoding> finish(Completion = Completion::octets);
    std::size_t cursor_bit() const noexcept { return cursor_; }
    const Usage &usage() const noexcept { return usage_; }
    const Error *first_error() const noexcept { return error_ ? &*error_ : nullptr; }
    // Unpublished staging view; unavailable as a completed encoding after error.
    // Passing any overlapping view to write_octets/open_type is rejected.
    std::span<const std::byte> staged_bytes() const noexcept { return output_; }
};
} // namespace nrforge::uper
#endif
