#include "runtime.hpp"
#include <algorithm>
#include <functional>
#include <limits>
namespace nrforge::uper {
namespace {
unsigned width(std::uint64_t max) noexcept { return static_cast<unsigned>(std::bit_width(max)); }
std::uint64_t ordered(std::int64_t v) noexcept {
    return std::bit_cast<std::uint64_t>(v) ^ (std::uint64_t{1} << 63);
}
std::int64_t signed_ordered(std::uint64_t v) noexcept {
    return std::bit_cast<std::int64_t>(v ^ (std::uint64_t{1} << 63));
}
bool valid(Size s) noexcept {
    return s.unconstrained ? !s.lower && !s.upper && !s.extensible : s.lower <= s.upper;
}
bool bit(std::span<const std::byte> b, std::size_t i) noexcept {
    return (std::to_integer<unsigned>(b[i / 8]) & (0x80u >> (i % 8))) != 0;
}
bool valid_bits(std::span<const std::byte> b, std::size_t n) noexcept {
    if (b.size() != detail::octets_for_bits(n))
        return false;
    return !n || n % 8 == 0 ||
           (std::to_integer<unsigned>(b.back()) & ((1u << (8 - n % 8)) - 1u)) == 0;
}
} // namespace
namespace detail {
std::size_t octets_for_bits(std::size_t bits) noexcept { return bits / 8 + (bits % 8 != 0); }
} // namespace detail
Reader::Reader(std::span<const std::byte> in, Limits l, std::size_t n) noexcept
    : input_(in), limits_(l), limit_(n) {}
Reader::Reader(Reader &&r) noexcept
    : input_(r.input_), limits_(r.limits_), usage_(r.usage_), cursor_(r.cursor_), limit_(r.limit_),
      error_(r.error_), finished_(r.finished_) {
    r.finished_ = true;
}
[[noreturn]] void Reader::fault(ErrorCode e) const { throw detail::Fault{{e, cursor_}}; }
[[noreturn]] void Writer::fault(ErrorCode e) const { throw detail::Fault{{e, cursor_}}; }
void Reader::live() const {
    if (error_)
        throw detail::Fault{*error_};
    if (finished_)
        fault(ErrorCode::invalid_state);
}
void Writer::live() const {
    if (error_)
        throw detail::Fault{*error_};
    if (finished_)
        fault(ErrorCode::invalid_state);
}
Result<Reader> Reader::make(std::span<const std::byte> in, Limits l, std::optional<std::size_t> n) {
    if (in.size() > l.max_input_octets || in.size() > std::numeric_limits<std::size_t>::max() / 8)
        return Result<Reader>::failure({ErrorCode::resource_limit, 0});
    auto bits = n.value_or(in.size() * 8);
    if (bits > in.size() * 8)
        return Result<Reader>::failure({ErrorCode::invalid_argument, 0});
    return Result<Reader>::success(Reader(in, l, bits));
}
void Reader::charge(std::size_t &used, std::size_t n, std::size_t max) {
    if (used > max || n > max - used)
        fault(ErrorCode::resource_limit);
    used += n;
}
void Writer::charge(std::size_t &used, std::size_t n, std::size_t max) {
    if (used > max || n > max - used)
        fault(ErrorCode::resource_limit);
    used += n;
}
std::uint64_t Reader::bits(unsigned n) {
    if (n > 64)
        fault(ErrorCode::invalid_argument);
    if (cursor_ > limit_ || n > limit_ - cursor_)
        fault(ErrorCode::truncated_input);
    if (cursor_ > limits_.max_wire_bits || n > limits_.max_wire_bits - cursor_)
        fault(ErrorCode::resource_limit);
    std::uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i)
        v = (v << 1) | static_cast<unsigned>(bit(input_, cursor_++));
    return v;
}
void Writer::bits(std::uint64_t v, unsigned n) {
    if (n > 64)
        fault(ErrorCode::invalid_argument);
    if (n < 64 && (v >> n) != 0)
        fault(ErrorCode::constraint_violation);
    if (cursor_ > limits_.max_wire_bits || n > limits_.max_wire_bits - cursor_)
        fault(ErrorCode::resource_limit);
    auto end = cursor_ + n, octets = detail::octets_for_bits(end);
    if (octets > limits_.max_output_octets)
        fault(ErrorCode::resource_limit);
    output_.resize(octets, std::byte{0});
    for (unsigned i = n; i > 0; --i) {
        auto mask = std::byte{static_cast<unsigned char>(0x80u >> (cursor_ % 8))};
        if ((v >> (i - 1)) & 1u)
            output_[cursor_ / 8] |= mask;
        else
            output_[cursor_ / 8] &= ~mask;
        ++cursor_;
    }
}
std::uint64_t Reader::bounded(std::uint64_t l, std::uint64_t u) {
    if (l > u)
        fault(ErrorCode::invalid_argument);
    auto v = bits(width(u - l));
    if (v > u - l)
        fault(ErrorCode::constraint_violation);
    return l + v;
}
void Writer::bounded(std::uint64_t v, std::uint64_t l, std::uint64_t u) {
    if (l > u)
        fault(ErrorCode::invalid_argument);
    if (v < l || v > u)
        fault(ErrorCode::constraint_violation);
    bits(v - l, width(u - l));
}
LengthSegment Reader::length() {
    auto first = bits(8);
    if (first < 128)
        return {static_cast<std::size_t>(first), false};
    if (first < 192) {
        auto n = ((first & 63) << 8) | bits(8);
        if (n < 128)
            fault(ErrorCode::constraint_violation);
        return {static_cast<std::size_t>(n), false};
    }
    auto m = first & 63;
    if (m < 1 || m > 4)
        fault(ErrorCode::constraint_violation);
    return {static_cast<std::size_t>(m) * 16384, true};
}
void Writer::length(std::size_t n, bool fragmented) {
    if (fragmented) {
        if (n < 16384 || n > 65536 || n % 16384)
            fault(ErrorCode::invalid_argument);
        bits(192 + n / 16384, 8);
    } else {
        if (n >= 16384)
            fault(ErrorCode::invalid_argument);
        bits(n < 128 ? n : 0x8000u + n, n < 128 ? 8 : 16);
    }
}
std::uint64_t Reader::small() {
    if (!bits(1))
        return bits(6);
    auto n = length();
    if (n.fragmented || !n.count || n.count > 8)
        fault(ErrorCode::resource_limit);
    auto v = bits(static_cast<unsigned>(n.count * 8));
    if (v < 64 || (n.count > 1 && v >> (8 * (n.count - 1)) == 0))
        fault(ErrorCode::constraint_violation);
    return v;
}
void Writer::small(std::uint64_t v) {
    if (v < 64) {
        bits(0, 1);
        bits(v, 6);
    } else {
        bits(1, 1);
        auto n = (width(v) + 7) / 8;
        length(n, false);
        bits(v, n * 8);
    }
}
std::uint64_t Reader::integer(bool is_unsigned) {
    auto n = length();
    if (n.fragmented || !n.count || n.count > (is_unsigned ? 9u : 8u))
        fault(ErrorCode::resource_limit);
    auto first = bits(8);
    if (is_unsigned && (first & 128))
        fault(ErrorCode::constraint_violation);
    std::uint64_t v = first;
    for (std::size_t i = 1; i < n.count; ++i) {
        auto next = bits(8);
        if (i == 1 && ((first == 0 && !(next & 128)) || (first == 255 && (next & 128))))
            fault(ErrorCode::constraint_violation);
        if (i == 1 && n.count == 9 && first != 0)
            fault(ErrorCode::constraint_violation);
        v = (v << 8) | next;
    }
    if (!is_unsigned && n.count < 8 && (first & 128))
        v |= (~std::uint64_t{0}) << (n.count * 8);
    return v;
}
void Writer::integer(std::uint64_t v, bool is_unsigned) {
    unsigned n = 8;
    if (is_unsigned) {
        n = std::max(1u, (width(v) + 7) / 8);
        if (v >> (n * 8 - 1))
            ++n;
    } else {
        while (n > 1) {
            auto a = (v >> ((n - 1) * 8)) & 255, b = (v >> ((n - 2) * 8)) & 255;
            if ((a == 0 && !(b & 128)) || (a == 255 && (b & 128)))
                --n;
            else
                break;
        }
    }
    length(n, false);
    if (n == 9) {
        bits(0, 8);
        bits(v, 64);
    } else {
        auto mask = n == 8 ? ~std::uint64_t{0} : (std::uint64_t{1} << (n * 8)) - 1;
        bits(v & mask, n * 8);
    }
}
BitString Reader::blob(Size s, bool octets, bool retain) {
    if (!valid(s))
        fault(ErrorCode::invalid_argument);
    bool ext = s.extensible && bits(1) != 0;
    bool general = s.unconstrained || ext || s.upper >= 65536;
    BitString result;
    std::size_t total = 0;
    do {
        LengthSegment seg =
            general ? length()
                    : LengthSegment{static_cast<std::size_t>(bounded(s.lower, s.upper)), false};
        if (seg.count > std::numeric_limits<std::size_t>::max() - total)
            fault(ErrorCode::resource_limit);
        auto next = total + seg.count;
        if (octets && next > std::numeric_limits<std::size_t>::max() / 8)
            fault(ErrorCode::resource_limit);
        auto end_bits = octets ? next * 8 : next;
        auto end_octets = detail::octets_for_bits(end_bits), old_octets = result.octets.size();
        charge(usage_.value_octets, end_octets - old_octets, limits_.max_value_octets);
        if (retain)
            charge(usage_.unknown_octets, end_octets - old_octets, limits_.max_unknown_octets);
        if (!s.unconstrained && !ext && next > s.upper)
            fault(ErrorCode::constraint_violation);
        // Preflight the complete segment before allocation, with units explicit.
        auto count_bits = octets ? seg.count * 8 : seg.count;
        if (cursor_ > limit_ || count_bits > limit_ - cursor_)
            fault(ErrorCode::truncated_input);
        if (cursor_ > limits_.max_wire_bits || count_bits > limits_.max_wire_bits - cursor_)
            fault(ErrorCode::resource_limit);
        result.octets.resize(end_octets, std::byte{0});
        if (octets)
            for (std::size_t i = total; i < next; ++i)
                result.octets[i] = std::byte{static_cast<unsigned char>(bits(8))};
        else
            for (std::size_t i = total; i < next; ++i)
                if (bits(1))
                    result.octets[i / 8] |= std::byte{static_cast<unsigned char>(0x80u >> (i % 8))};
        total = next;
        if (!seg.fragmented)
            break;
    } while (true);
    if (!s.unconstrained &&
        (!ext ? (total < s.lower || total > s.upper) : (total >= s.lower && total <= s.upper)))
        fault(ErrorCode::constraint_violation);
    result.bit_count = octets ? total * 8 : total;
    return result;
}
void Writer::blob(std::span<const std::byte> value, std::size_t count, Size s, bool octets) {
    // Reject staging aliases before any resize can invalidate the input span.
    const std::less<const std::byte *> less;
    if (!value.empty() && !output_.empty() && less(value.data(), output_.data() + output_.size()) &&
        less(output_.data(), value.data() + value.size()))
        fault(ErrorCode::invalid_argument);
    if (!valid(s) || (!octets && !valid_bits(value, count)) || (octets && count != value.size()))
        fault(ErrorCode::invalid_argument);
    bool ext = !s.unconstrained && (count < s.lower || count > s.upper);
    if (ext && !s.extensible)
        fault(ErrorCode::constraint_violation);
    if (s.extensible)
        bits(ext, 1);
    bool general = s.unconstrained || ext || s.upper >= 65536;
    charge(usage_.value_octets, value.size(), limits_.max_value_octets);
    std::size_t done = 0;
    do {
        auto remaining = count - done;
        bool fragment = general && remaining >= 16384;
        auto n = fragment ? std::min(std::size_t{4}, remaining / 16384) * 16384 : remaining;
        if (general)
            length(n, fragment);
        else
            bounded(n, s.lower, s.upper);
        if (octets)
            for (std::size_t i = done; i < done + n; ++i)
                bits(std::to_integer<unsigned>(value[i]), 8);
        else
            for (std::size_t i = done; i < done + n; ++i)
                bits(bit(value, i), 1);
        done += n;
        if (!fragment)
            break;
    } while (true);
}
Result<std::uint64_t> Reader::read_bits(unsigned n) {
    return transaction<std::uint64_t>([&] { return bits(n); });
}
Result<void> Writer::write_bits(std::uint64_t v, unsigned n) {
    return transaction([&] { bits(v, n); });
}
Result<std::uint64_t> Reader::read_uint(std::uint64_t l, std::uint64_t u) {
    return transaction<std::uint64_t>([&] { return bounded(l, u); });
}
Result<void> Writer::write_uint(std::uint64_t v, std::uint64_t l, std::uint64_t u) {
    return transaction([&] { bounded(v, l, u); });
}
Result<std::int64_t> Reader::read_int(std::int64_t l, std::int64_t u) {
    return transaction<std::int64_t>(
        [&] { return signed_ordered(bounded(ordered(l), ordered(u))); });
}
Result<void> Writer::write_int(std::int64_t v, std::int64_t l, std::int64_t u) {
    return transaction([&] { bounded(ordered(v), ordered(l), ordered(u)); });
}
Result<std::int64_t> Reader::read_unconstrained_int() {
    return transaction<std::int64_t>([&] { return std::bit_cast<std::int64_t>(integer(false)); });
}
Result<void> Writer::write_unconstrained_int(std::int64_t v) {
    return transaction([&] { integer(std::bit_cast<std::uint64_t>(v), false); });
}
Result<std::uint64_t> Reader::read_extensible_uint(std::uint64_t l, std::uint64_t u) {
    return transaction<std::uint64_t>([&] {
        if (l > u)
            fault(ErrorCode::invalid_argument);
        if (!bits(1))
            return bounded(l, u);
        auto v = integer(true);
        if (v >= l && v <= u)
            fault(ErrorCode::constraint_violation);
        return v;
    });
}
Result<void> Writer::write_extensible_uint(std::uint64_t v, std::uint64_t l, std::uint64_t u) {
    return transaction([&] {
        if (l > u)
            fault(ErrorCode::invalid_argument);
        bool ext = v < l || v > u;
        bits(ext, 1);
        if (ext)
            integer(v, true);
        else
            bounded(v, l, u);
    });
}
Result<std::int64_t> Reader::read_extensible_int(std::int64_t l, std::int64_t u) {
    return transaction<std::int64_t>([&] {
        if (l > u)
            fault(ErrorCode::invalid_argument);
        if (!bits(1))
            return signed_ordered(bounded(ordered(l), ordered(u)));
        auto v = std::bit_cast<std::int64_t>(integer(false));
        if (v >= l && v <= u)
            fault(ErrorCode::constraint_violation);
        return v;
    });
}
Result<void> Writer::write_extensible_int(std::int64_t v, std::int64_t l, std::int64_t u) {
    return transaction([&] {
        if (l > u)
            fault(ErrorCode::invalid_argument);
        bool ext = v < l || v > u;
        bits(ext, 1);
        if (ext)
            integer(std::bit_cast<std::uint64_t>(v), false);
        else
            bounded(ordered(v), ordered(l), ordered(u));
    });
}
Result<std::uint64_t> Reader::read_normally_small() {
    return transaction<std::uint64_t>([&] { return small(); });
}
Result<void> Writer::write_normally_small(std::uint64_t v) {
    return transaction([&] { small(v); });
}
Result<Index> Reader::read_index(std::uint64_t roots, bool extensible) {
    return transaction<Index>([&] {
        if (!roots)
            fault(ErrorCode::invalid_argument);
        bool ext = extensible && bits(1);
        return Index{ext, ext ? small() : bounded(0, roots - 1)};
    });
}
Result<void> Writer::write_index(Index v, std::uint64_t roots, bool extensible) {
    return transaction([&] {
        if (!roots || (!extensible && v.extension))
            fault(ErrorCode::invalid_argument);
        if (extensible)
            bits(v.extension, 1);
        if (v.extension)
            small(v.index);
        else
            bounded(v.index, 0, roots - 1);
    });
}
Result<LengthSegment> Reader::read_collection_segment() {
    return transaction<LengthSegment>([&] {
        auto s = length();
        charge(usage_.collection_elements, s.count, limits_.max_collection_elements);
        return s;
    });
}
Result<void> Writer::write_collection_segment(std::size_t n, bool fragment) {
    return transaction([&] {
        charge(usage_.collection_elements, n, limits_.max_collection_elements);
        length(n, fragment);
    });
}
Result<std::size_t> Reader::read_collection_length(std::size_t l, std::size_t u) {
    return transaction<std::size_t>([&] {
        if (u >= 65536)
            fault(ErrorCode::invalid_argument);
        auto n = static_cast<std::size_t>(bounded(l, u));
        charge(usage_.collection_elements, n, limits_.max_collection_elements);
        return n;
    });
}
Result<void> Writer::write_collection_length(std::size_t n, std::size_t l, std::size_t u) {
    return transaction([&] {
        if (u >= 65536)
            fault(ErrorCode::invalid_argument);
        charge(usage_.collection_elements, n, limits_.max_collection_elements);
        bounded(n, l, u);
    });
}
Result<std::vector<std::byte>> Reader::read_octets(Size s) {
    return transaction<std::vector<std::byte>>([&] { return blob(s, true).octets; });
}
Result<void> Writer::write_octets(std::span<const std::byte> v, Size s) {
    return transaction([&] { blob(v, v.size(), s, true); });
}
Result<BitString> Reader::read_bit_string(Size s) {
    return transaction<BitString>([&] { return blob(s, false); });
}
Result<void> Writer::write_bit_string(const BitString &v, Size s) {
    return transaction([&] { blob(v.octets, v.bit_count, s, false); });
}
Result<BitString> Reader::read_extension_bitmap() {
    return transaction<BitString>([&] {
        std::size_t n;
        if (!bits(1))
            n = static_cast<std::size_t>(bits(6)) + 1;
        else {
            auto s = length();
            if (s.fragmented || s.count <= 64)
                fault(ErrorCode::constraint_violation);
            n = s.count;
        }
        if (n >= 16384)
            fault(ErrorCode::resource_limit);
        charge(usage_.bitmap_bits, n, limits_.max_bitmap_bits);
        return blob(Size{n, n}, false);
    });
}
Result<void> Writer::write_extension_bitmap(const BitString &v) {
    return transaction([&] {
        if (!v.bit_count || v.bit_count >= 16384)
            fault(ErrorCode::invalid_argument);
        charge(usage_.bitmap_bits, v.bit_count, limits_.max_bitmap_bits);
        if (v.bit_count <= 64) {
            bits(0, 1);
            bits(v.bit_count - 1, 6);
        } else {
            bits(1, 1);
            length(v.bit_count, false);
        }
        blob(v.octets, v.bit_count, Size{v.bit_count, v.bit_count}, false);
    });
}
Result<std::vector<std::byte>> Reader::read_open_type(bool retain) {
    return transaction<std::vector<std::byte>>([&] {
        if (retain)
            charge(usage_.unknown_records, 1, limits_.max_unknown_records);
        auto b = blob(Size{0, 0, true}, true, retain);
        if (b.octets.empty())
            fault(ErrorCode::constraint_violation);
        return std::move(b.octets);
    });
}
Result<void> Writer::write_open_type(std::span<const std::byte> v) {
    return transaction([&] {
        if (v.empty())
            fault(ErrorCode::invalid_argument);
        blob(v, v.size(), Size{0, 0, true}, true);
    });
}
Result<void> Reader::skip_open_type() {
    auto r = transaction<unsigned>([&] {
        std::size_t total = 0;
        do {
            auto seg = length();
            if (seg.count > std::numeric_limits<std::size_t>::max() - total)
                fault(ErrorCode::resource_limit);
            total += seg.count;
            if (total > limits_.max_value_octets)
                fault(ErrorCode::resource_limit);
            if (seg.count > std::numeric_limits<std::size_t>::max() / 8)
                fault(ErrorCode::resource_limit);
            auto n = seg.count * 8;
            if (cursor_ > limit_ || n > limit_ - cursor_)
                fault(ErrorCode::truncated_input);
            if (cursor_ > limits_.max_wire_bits || n > limits_.max_wire_bits - cursor_)
                fault(ErrorCode::resource_limit);
            cursor_ += n;
            if (!seg.fragmented)
                break;
        } while (true);
        if (!total)
            fault(ErrorCode::constraint_violation);
        return 0u;
    });
    return r ? Result<void>::success() : Result<void>::failure(r.error());
}
Result<void> Reader::finish(Completion form) {
    auto r = transaction<unsigned>([&] {
        if (form != Completion::octets && form != Completion::contained_bits &&
            form != Completion::exact)
            fault(ErrorCode::invalid_argument);
        auto expected = cursor_;
        if (form == Completion::octets)
            expected = std::max(std::size_t{1}, detail::octets_for_bits(cursor_)) * 8;
        else if (form == Completion::contained_bits)
            expected = std::max(std::size_t{1}, cursor_);
        if (limit_ != expected || input_.size() != detail::octets_for_bits(expected))
            fault(ErrorCode::trailing_data);
        if (cursor_ > limits_.max_wire_bits || expected - cursor_ > limits_.max_wire_bits - cursor_)
            fault(ErrorCode::resource_limit);
        for (std::size_t i = cursor_; i < input_.size() * 8; ++i)
            if (bit(input_, i))
                fault(ErrorCode::nonzero_padding);
        finished_ = true;
        return 0u;
    });
    return r ? Result<void>::success() : Result<void>::failure(r.error());
}
Result<CompleteEncoding> Writer::finish(Completion form) {
    CompleteEncoding result;
    auto r = transaction([&] {
        if (form != Completion::octets && form != Completion::contained_bits &&
            form != Completion::exact)
            fault(ErrorCode::invalid_argument);
        result.semantic_bits = cursor_;
        if (form == Completion::octets) {
            auto n = cursor_ ? static_cast<unsigned>((8 - cursor_ % 8) % 8) : 8u;
            bits(0, n);
            result.padding_bits = result.semantic_bits ? n : 0;
        } else if (form == Completion::contained_bits && !cursor_)
            bits(0, 1);
        result.empty_substitution = !result.semantic_bits && form != Completion::exact;
        result.encoded_bits = cursor_;
        result.octets = std::move(output_);
        finished_ = true;
    });
    return r ? Result<CompleteEncoding>::success(std::move(result))
             : Result<CompleteEncoding>::failure(r.error());
}
} // namespace nrforge::uper
