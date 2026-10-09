#include "runtime.hpp"

#include <limits>
#include <stdexcept>

namespace nrforge::aper {
namespace {

using detail::checked_add_size;
using detail::checked_bits_to_octets;
using detail::checked_octets_to_bits;

unsigned get_bit(std::span<const std::byte> bytes, std::size_t position) noexcept {
    const auto byte = std::to_integer<unsigned>(bytes[position / 8]);
    return (byte >> (7u - static_cast<unsigned>(position % 8))) & 1u;
}

void set_bit(std::vector<std::byte>& bytes, std::size_t position, bool value) noexcept {
    const auto mask = static_cast<unsigned char>(1u << (7u - (position % 8)));
    auto byte = std::to_integer<unsigned char>(bytes[position / 8]);
    byte = value ? static_cast<unsigned char>(byte | mask)
                 : static_cast<unsigned char>(byte & static_cast<unsigned char>(~mask));
    bytes[position / 8] = static_cast<std::byte>(byte);
}

bool constrained_width_supported(unsigned bits) noexcept {
    return bits == 8 || bits == 16 || bits == 32 || bits == 40;
}

} // namespace

bool detail::checked_add_size(std::size_t left, std::size_t right,
                              std::size_t& out) noexcept {
    if(left > std::numeric_limits<std::size_t>::max() - right) return false;
    out = left + right;
    return true;
}

bool detail::checked_bits_to_octets(std::size_t bits, std::size_t& octets) noexcept {
    octets = bits / 8;
    if(bits % 8 != 0) ++octets;
    return true;
}

bool detail::checked_octets_to_bits(std::size_t octets, std::size_t& bits) noexcept {
    if(octets > std::numeric_limits<std::size_t>::max() / 8) return false;
    bits = octets * 8;
    return true;
}

BitReader::BitReader(std::span<const std::byte> input, DecodeContext& context,
                     std::size_t logical_bit_limit) noexcept
    : input_(input), context_(&context), logical_bit_limit_(logical_bit_limit) {}

Result<BitReader> BitReader::make(std::span<const std::byte> input,
                                  DecodeContext& context) {
    if(context.failed_) return Result<BitReader>::failure(context.error_);
    if(context.finished_)
        return Result<BitReader>::failure({ErrorCode::invalid_state, 0});
    std::size_t input_bits = 0;
    if(input.size() > context.limits_.max_input_octets || !checked_octets_to_bits(input.size(), input_bits)) {
        Error error{ErrorCode::resource_limit, 0};
        context.fail(error);
        return Result<BitReader>::failure(error);
    }
    return Result<BitReader>::success(BitReader(input, context, input_bits));
}

Result<BitReader> BitReader::make_bounded_for_test(std::span<const std::byte> input,
                                                   std::size_t logical_bit_limit,
                                                   DecodeContext& context) {
    if(context.failed_) return Result<BitReader>::failure(context.error_);
    if(context.finished_)
        return Result<BitReader>::failure({ErrorCode::invalid_state, 0});
    std::size_t input_bits = 0;
    if(input.size() > context.limits_.max_input_octets || !checked_octets_to_bits(input.size(), input_bits)) {
        Error error{ErrorCode::resource_limit, 0};
        context.fail(error);
        return Result<BitReader>::failure(error);
    }
    if(logical_bit_limit > input_bits) {
        Error error{ErrorCode::invalid_argument, 0};
        context.fail(error);
        return Result<BitReader>::failure(error);
    }
    return Result<BitReader>::success(BitReader(input, context, logical_bit_limit));
}

BitReader::BitReader(BitReader&& other) noexcept
    : input_(other.input_), context_(other.context_), logical_bit_limit_(other.logical_bit_limit_),
      cursor_bit_(other.cursor_bit_) {
    other.context_ = nullptr;
}

Result<void> BitReader::fail(Error error) noexcept {
    if(context_) context_->fail(error);
    return Result<void>::failure(context_ && context_->failed_ ? context_->error_ : error);
}

Result<void> BitReader::validate_live() const noexcept {
    if(!context_) return Result<void>::failure({ErrorCode::invalid_state, cursor_bit_});
    if(context_->failed_) return Result<void>::failure(context_->error_);
    if(context_->finished_) return Result<void>::failure({ErrorCode::invalid_state, cursor_bit_});
    return Result<void>::success();
}

Result<void> BitReader::preflight(std::size_t bit_count, std::size_t start) noexcept {
    auto live = validate_live();
    if(!live) return live;
    if(start > logical_bit_limit_ || bit_count > logical_bit_limit_ - start)
        return fail({ErrorCode::truncated_input, logical_bit_limit_});
    std::size_t projected = 0;
    if(!checked_add_size(context_->wire_bits_, bit_count, projected) ||
       projected > context_->limits_.max_wire_bits)
        return fail({ErrorCode::resource_limit, start});
    return Result<void>::success();
}

Result<bool> BitReader::read_bit() {
    auto ready = preflight(1, cursor_bit_);
    if(!ready) return Result<bool>::failure(ready.error());
    const bool value = get_bit(input_, cursor_bit_) != 0;
    ++cursor_bit_;
    ++context_->wire_bits_;
    return Result<bool>::success(value);
}

Result<void> BitReader::align_to_octet_zero() {
    auto live = validate_live();
    if(!live) return live;
    const auto padding = (8 - (cursor_bit_ % 8)) % 8;
    auto ready = preflight(padding, cursor_bit_);
    if(!ready) return ready;
    for(std::size_t i = 0; i < padding; ++i) {
        if(get_bit(input_, cursor_bit_ + i) != 0)
            return fail({ErrorCode::nonzero_padding, cursor_bit_ + i});
    }
    cursor_bit_ += padding;
    context_->wire_bits_ += padding;
    return Result<void>::success();
}

Result<std::uint16_t> BitReader::read_aligned_u16_be() {
    auto live = validate_live();
    if(!live) return Result<std::uint16_t>::failure(live.error());
    const auto start = cursor_bit_;
    const auto padding = (8 - (start % 8)) % 8;
    std::size_t total = 0;
    if(!checked_add_size(padding, 16, total))
        return Result<std::uint16_t>::failure(fail({ErrorCode::resource_limit, start}).error());
    auto ready = preflight(total, start);
    if(!ready) return Result<std::uint16_t>::failure(ready.error());
    for(std::size_t i = 0; i < padding; ++i) {
        if(get_bit(input_, start + i) != 0)
            return Result<std::uint16_t>::failure(fail({ErrorCode::nonzero_padding, start + i}).error());
    }
    const auto payload_start = start + padding;
    std::uint32_t accumulator = 0;
    for(std::size_t i = 0; i < 16; ++i)
        accumulator = (accumulator << 1) | get_bit(input_, payload_start + i);
    cursor_bit_ = payload_start + 16;
    context_->wire_bits_ += total;
    return Result<std::uint16_t>::success(static_cast<std::uint16_t>(accumulator));
}

Result<void> BitReader::validate_complete_value() {
    auto live = validate_live();
    if(!live) return live;
    if(cursor_bit_ == 0) {
        if(input_.empty()) return fail({ErrorCode::truncated_input, 0});
        std::size_t projected_wire = 0;
        if(!checked_add_size(context_->wire_bits_, 8, projected_wire) ||
           projected_wire > context_->limits_.max_wire_bits)
            return fail({ErrorCode::resource_limit, 0});
        for(std::size_t i = 0; i < 8; ++i)
            if(get_bit(input_, i) != 0) return fail({ErrorCode::nonzero_padding, i});
        if(input_.size() > 1) return fail({ErrorCode::trailing_data, 8});
        context_->wire_bits_ = projected_wire;
        cursor_bit_ = 8;
        context_->finished_ = true;
        return Result<void>::success();
    }

    const auto padding = (8 - (cursor_bit_ % 8)) % 8;
    auto ready = preflight(padding, cursor_bit_);
    if(!ready) return ready;
    for(std::size_t i = 0; i < padding; ++i)
        if(get_bit(input_, cursor_bit_ + i) != 0)
            return fail({ErrorCode::nonzero_padding, cursor_bit_ + i});
    const auto rounded_end = cursor_bit_ + padding;
    if(rounded_end / 8 < input_.size()) return fail({ErrorCode::trailing_data, rounded_end});
    cursor_bit_ = rounded_end;
    context_->wire_bits_ += padding;
    context_->finished_ = true;
    return Result<void>::success();
}

Result<std::uint64_t> BitReader::read_constrained_uint(unsigned root_bits) {
    auto live = validate_live();
    if(!live) return Result<std::uint64_t>::failure(live.error());
    const auto start = cursor_bit_;
    if(!constrained_width_supported(root_bits))
        return Result<std::uint64_t>::failure(fail({ErrorCode::invalid_argument, start}).error());
    if(root_bits == 16) {
        auto value = read_aligned_u16_be();
        return value ? Result<std::uint64_t>::success(value.value())
                     : Result<std::uint64_t>::failure(value.error());
    }

    const unsigned prefix_bits = root_bits == 32 ? 2u : (root_bits == 40 ? 3u : 0u);
    unsigned octets = 1;
    // Peek the prefix without a budget debit or cursor publication. Invalid
    // selectors have the explicit N1 priority over payload availability/budget.
    if(prefix_bits) {
        if(start > logical_bit_limit_ || prefix_bits > logical_bit_limit_ - start)
            return Result<std::uint64_t>::failure(fail({ErrorCode::truncated_input, logical_bit_limit_}).error());
        unsigned selector = 0;
        for(unsigned i = 0; i < prefix_bits; ++i)
            selector = (selector << 1) | get_bit(input_, start + i);
        octets = selector + 1;
        if(octets > root_bits / 8)
            return Result<std::uint64_t>::failure(fail({ErrorCode::constraint_violation, start}).error());
    }
    std::size_t after_prefix = 0;
    std::size_t total = 0;
    if(!checked_add_size(start, prefix_bits, after_prefix))
        return Result<std::uint64_t>::failure(fail({ErrorCode::resource_limit, start}).error());
    const auto padding = (8 - (after_prefix % 8)) % 8;
    if(!checked_add_size(prefix_bits, padding + octets * std::size_t{8}, total))
        return Result<std::uint64_t>::failure(fail({ErrorCode::resource_limit, start}).error());
    auto ready = preflight(total, start);
    if(!ready) return Result<std::uint64_t>::failure(ready.error());
    for(std::size_t i = 0; i < padding; ++i)
        if(get_bit(input_, after_prefix + i))
            return Result<std::uint64_t>::failure(fail({ErrorCode::nonzero_padding, after_prefix + i}).error());
    const auto payload_start = after_prefix + padding;
    std::uint64_t value = 0;
    for(std::size_t i = 0; i < octets * std::size_t{8}; ++i)
        value = (value << 1) | get_bit(input_, payload_start + i);
    if(prefix_bits && octets > 1 && (value >> ((octets - 1) * 8)) == 0)
        return Result<std::uint64_t>::failure(fail({ErrorCode::constraint_violation, start}).error());
    cursor_bit_ = start + total;
    context_->wire_bits_ += total;
    return Result<std::uint64_t>::success(value);
}

BitWriter::BitWriter(BitWriter&& other) noexcept
    : context_(other.context_), output_(std::move(other.output_)), cursor_bit_(other.cursor_bit_) {
    other.context_ = nullptr;
    other.cursor_bit_ = 0;
}

Result<void> BitWriter::fail(Error error) noexcept {
    if(context_) context_->fail(error);
    return Result<void>::failure(context_ && context_->failed_ ? context_->error_ : error);
}

Result<void> BitWriter::validate_live() const noexcept {
    if(!context_) return Result<void>::failure({ErrorCode::invalid_state, cursor_bit_});
    if(context_->failed_) return Result<void>::failure(context_->error_);
    if(context_->finished_) return Result<void>::failure({ErrorCode::invalid_state, cursor_bit_});
    return Result<void>::success();
}

Result<void> BitWriter::preflight(std::size_t bit_count, std::size_t projected_end,
                                  std::size_t start) noexcept {
    auto live = validate_live();
    if(!live) return live;
    std::size_t projected_wire = 0;
    std::size_t projected_octets = 0;
    if(!checked_add_size(context_->wire_bits_, bit_count, projected_wire) ||
       projected_wire > context_->limits_.max_wire_bits ||
       !checked_bits_to_octets(projected_end, projected_octets) ||
       projected_octets > context_->limits_.max_output_octets)
        return fail({ErrorCode::resource_limit, start});
    if(projected_octets > output_.max_size())
        return fail({ErrorCode::resource_limit, start});
    return Result<void>::success();
}

Result<void> BitWriter::grow_to(std::size_t octets, std::size_t start) noexcept {
    if(octets <= output_.size()) return Result<void>::success();
    if(octets > output_.max_size()) return fail({ErrorCode::resource_limit, start});
    try {
        output_.resize(octets, std::byte{0});
    } catch(const std::bad_alloc&) {
        return fail({ErrorCode::allocation_failure, start});
    } catch(const std::length_error&) {
        return fail({ErrorCode::resource_limit, start});
    }
    return Result<void>::success();
}

Result<void> BitWriter::write_bit(bool value) {
    auto live = validate_live();
    if(!live) return live;
    const auto start = cursor_bit_;
    std::size_t end = 0;
    if(!checked_add_size(start, 1, end)) return fail({ErrorCode::resource_limit, start});
    auto ready = preflight(1, end, start);
    if(!ready) return ready;
    std::size_t octets = 0;
    (void)checked_bits_to_octets(end, octets);
    auto grown = grow_to(octets, start);
    if(!grown) return grown;
    set_bit(output_, start, value);
    cursor_bit_ = end;
    context_->wire_bits_ += 1;
    context_->logical_output_octets_ = octets;
    return Result<void>::success();
}

Result<void> BitWriter::align_to_octet_zero() {
    auto live = validate_live();
    if(!live) return live;
    const auto start = cursor_bit_;
    const auto padding = (8 - (start % 8)) % 8;
    std::size_t end = 0;
    if(!checked_add_size(start, padding, end)) return fail({ErrorCode::resource_limit, start});
    auto ready = preflight(padding, end, start);
    if(!ready) return ready;
    std::size_t octets = 0;
    (void)checked_bits_to_octets(end, octets);
    auto grown = grow_to(octets, start);
    if(!grown) return grown;
    for(std::size_t i = 0; i < padding; ++i) set_bit(output_, start + i, false);
    cursor_bit_ = end;
    context_->wire_bits_ += padding;
    context_->logical_output_octets_ = octets;
    return Result<void>::success();
}

Result<void> BitWriter::write_aligned_u16_be(std::uint64_t value) {
    auto live = validate_live();
    if(!live) return live;
    const auto start = cursor_bit_;
    if(value > 65535) return fail({ErrorCode::constraint_violation, start});
    const auto padding = (8 - (start % 8)) % 8;
    std::size_t total = 0;
    std::size_t end = 0;
    if(!checked_add_size(padding, 16, total) || !checked_add_size(start, total, end))
        return fail({ErrorCode::resource_limit, start});
    auto ready = preflight(total, end, start);
    if(!ready) return ready;
    std::size_t octets = 0;
    (void)checked_bits_to_octets(end, octets);
    auto grown = grow_to(octets, start);
    if(!grown) return grown;
    for(std::size_t i = 0; i < padding; ++i) set_bit(output_, start + i, false);
    const auto payload_start = start + padding;
    for(std::size_t i = 0; i < 16; ++i) {
        const auto shift = static_cast<unsigned>(15 - i);
        set_bit(output_, payload_start + i, ((value >> shift) & 1u) != 0);
    }
    cursor_bit_ = end;
    context_->wire_bits_ += total;
    context_->logical_output_octets_ = octets;
    return Result<void>::success();
}

Result<CompleteEncoding> BitWriter::finish() {
    const auto start = cursor_bit_;
    auto live = validate_live();
    if(!live) return Result<CompleteEncoding>::failure(live.error());
    const bool empty = cursor_bit_ == 0;
    const auto padding = empty ? std::size_t{8} : (8 - (cursor_bit_ % 8)) % 8;
    std::size_t end = 0;
    if(!checked_add_size(cursor_bit_, padding, end))
        return Result<CompleteEncoding>::failure(fail({ErrorCode::resource_limit, start}).error());
    auto ready = preflight(padding, end, start);
    if(!ready) return Result<CompleteEncoding>::failure(ready.error());
    std::size_t octets = 0;
    (void)checked_bits_to_octets(end, octets);
    auto grown = grow_to(octets, start);
    if(!grown) return Result<CompleteEncoding>::failure(grown.error());
    for(std::size_t i = 0; i < padding; ++i) set_bit(output_, cursor_bit_ + i, false);

    CompleteEncoding result;
    result.last_field_end_bit = cursor_bit_;
    result.final_padding_bits = static_cast<std::uint8_t>(empty ? 0 : padding);
    result.empty_encoding_substitution = empty;
    result.complete_encoding_bits = end;
    result.octet_count = octets;
    result.octets = std::move(output_);
    context_->wire_bits_ += padding;
    context_->logical_output_octets_ = octets;
    context_->finished_ = true;
    cursor_bit_ = end;
    return Result<CompleteEncoding>::success(std::move(result));
}

Result<void> BitWriter::write_constrained_uint(std::uint64_t value, unsigned root_bits) {
    auto live = validate_live();
    if(!live) return live;
    const auto start = cursor_bit_;
    if(!constrained_width_supported(root_bits))
        return fail({ErrorCode::invalid_argument, start});
    if(root_bits == 16) return write_aligned_u16_be(value);
    if(value > ((std::uint64_t{1} << root_bits) - 1))
        return fail({ErrorCode::constraint_violation, start});
    const unsigned prefix_bits = root_bits == 32 ? 2u : (root_bits == 40 ? 3u : 0u);
    unsigned octets = 1;
    if(prefix_bits)
        for(auto rest = value >> 8; rest; rest >>= 8) ++octets;
    std::size_t after_prefix = 0;
    std::size_t total = 0;
    std::size_t end = 0;
    if(!checked_add_size(start, prefix_bits, after_prefix))
        return fail({ErrorCode::resource_limit, start});
    const auto padding = (8 - (after_prefix % 8)) % 8;
    if(!checked_add_size(prefix_bits, padding + octets * std::size_t{8}, total) ||
       !checked_add_size(start, total, end))
        return fail({ErrorCode::resource_limit, start});
    auto ready = preflight(total, end, start);
    if(!ready) return ready;
    std::size_t output_octets = 0;
    (void)checked_bits_to_octets(end, output_octets);
    auto grown = grow_to(output_octets, start);
    if(!grown) return grown;
    for(unsigned i = 0; i < prefix_bits; ++i)
        set_bit(output_, start + i, (((octets - 1) >> (prefix_bits - i - 1)) & 1u) != 0);
    for(std::size_t i = 0; i < padding; ++i)
        set_bit(output_, after_prefix + i, false);
    const auto payload_start = after_prefix + padding;
    for(unsigned i = 0; i < octets * 8; ++i)
        set_bit(output_, payload_start + i, ((value >> (octets * 8 - i - 1)) & 1u) != 0);
    cursor_bit_ = end;
    context_->wire_bits_ += total;
    context_->logical_output_octets_ = output_octets;
    return Result<void>::success();
}


Result<void> BitWriter::write_enumerated(EnumeratedIndex value,
                                         unsigned root_count, bool extensible) {
    auto live = validate_live();
    if(!live) return live;
    const auto start = cursor_bit_;
    if(root_count == 0 || root_count > 255)
        return fail({ErrorCode::invalid_argument, start});
    if((value.is_extension && !extensible) ||
       (!value.is_extension && value.index >= root_count))
        return fail({ErrorCode::constraint_violation, start});
    unsigned root_bits = 0;
    for(auto remaining = root_count - 1; remaining; remaining >>= 1) ++root_bits;
    const bool long_form = value.is_extension && value.index >= 64;
    const unsigned prefix_bits = value.is_extension ? (long_form ? 2u : 8u)
                                                    : (root_bits + (extensible ? 1u : 0u));
    unsigned octets = 1;
    if(long_form)
        for(auto remaining = value.index >> 8; remaining; remaining >>= 8) ++octets;
    std::size_t after_prefix = 0;
    std::size_t total = prefix_bits;
    std::size_t end = 0;
    if(!checked_add_size(start, prefix_bits, after_prefix))
        return fail({ErrorCode::resource_limit, start});
    const auto padding = long_form ? (8 - after_prefix % 8) % 8 : 0;
    if((long_form && !checked_add_size(total, padding + 8 + octets * std::size_t{8}, total)) ||
       !checked_add_size(start, total, end))
        return fail({ErrorCode::resource_limit, start});
    auto ready = preflight(total, end, start);
    if(!ready) return ready;
    std::size_t output_octets = 0;
    (void)checked_bits_to_octets(end, output_octets);
    auto grown = grow_to(output_octets, start);
    if(!grown) return grown;
    auto position = start;
    auto emit = [&](std::uint64_t number, unsigned width) {
        for(unsigned i = 0; i < width; ++i)
            set_bit(output_, position++, ((number >> (width - i - 1)) & 1u) != 0);
    };
    if(extensible) emit(value.is_extension ? 1u : 0u, 1);
    if(!value.is_extension) {
        emit(value.index, root_bits);
    } else {
        emit(long_form ? 1u : 0u, 1);
        if(!long_form) {
            emit(value.index, 6);
        } else {
            for(std::size_t i = 0; i < padding; ++i) emit(0, 1);
            emit(octets, 8);
            emit(value.index, octets * 8);
        }
    }
    cursor_bit_ = end;
    context_->wire_bits_ += total;
    context_->logical_output_octets_ = output_octets;
    return Result<void>::success();
}

Result<EnumeratedIndex> BitReader::read_enumerated(unsigned root_count, bool extensible) {
    auto live = validate_live();
    if(!live) return Result<EnumeratedIndex>::failure(live.error());
    const auto start = cursor_bit_;
    auto reject = [&](ErrorCode code, std::size_t offset) {
        return Result<EnumeratedIndex>::failure(fail({code, offset}).error());
    };
    if(root_count == 0 || root_count > 255)
        return reject(ErrorCode::invalid_argument, start);
    auto peek = [&](std::size_t position, unsigned width, std::uint64_t& number) {
        if(position > logical_bit_limit_ || width > logical_bit_limit_ - position) return false;
        number = 0;
        for(unsigned i = 0; i < width; ++i)
            number = (number << 1) | get_bit(input_, position + i);
        return true;
    };
    auto position = start;
    std::uint64_t flag = 0;
    if(extensible) {
        if(!peek(position, 1, flag)) return reject(ErrorCode::truncated_input, logical_bit_limit_);
        ++position;
    }
    EnumeratedIndex value{flag != 0, 0};
    unsigned index_bits = 0;
    std::size_t padding_start = position;
    std::size_t padding = 0;
    bool long_form = false;
    if(!value.is_extension) {
        for(auto remaining = root_count - 1; remaining; remaining >>= 1) ++index_bits;
    } else {
        if(!peek(position, 1, flag)) return reject(ErrorCode::truncated_input, logical_bit_limit_);
        ++position;
        long_form = flag != 0;
        if(!long_form) {
            index_bits = 6;
        } else {
            padding_start = position;
            padding = (8 - position % 8) % 8;
            if(!checked_add_size(position, padding, position))
                return reject(ErrorCode::resource_limit, start);
            std::uint64_t length = 0;
            if(!peek(position, 8, length)) return reject(ErrorCode::truncated_input, logical_bit_limit_);
            position += 8;
            if((length & 0xc0u) == 0xc0u) return reject(ErrorCode::resource_limit, start);
            if(length & 0x80u) {
                std::uint64_t low = 0;
                if(!peek(position, 8, low)) return reject(ErrorCode::truncated_input, logical_bit_limit_);
                length = ((length & 0x3fu) << 8) | low;
                return reject(length > 8 ? ErrorCode::resource_limit : ErrorCode::constraint_violation, start);
            }
            if(length == 0) return reject(ErrorCode::constraint_violation, start);
            if(length > 8) return reject(ErrorCode::resource_limit, start);
            index_bits = static_cast<unsigned>(length) * 8;
        }
    }
    std::size_t end = 0;
    if(!checked_add_size(position, index_bits, end)) return reject(ErrorCode::resource_limit, start);
    auto ready = preflight(end - start, start);
    if(!ready) return Result<EnumeratedIndex>::failure(ready.error());
    for(std::size_t i = 0; i < padding; ++i)
        if(get_bit(input_, padding_start + i)) return reject(ErrorCode::nonzero_padding, padding_start + i);
    (void)peek(position, index_bits, value.index);
    if((!value.is_extension && value.index >= root_count) ||
       (long_form && (value.index < 64 ||
         (index_bits > 8 && (value.index >> (index_bits - 8)) == 0))))
        return reject(ErrorCode::constraint_violation, start);
    cursor_bit_ = end;
    context_->wire_bits_ += end - start;
    return Result<EnumeratedIndex>::success(value);
}

} // namespace nrforge::aper
