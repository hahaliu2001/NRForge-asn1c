#include "runtime.hpp"
#include <algorithm>

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
    if(context.finished_ || context.known_active_reader_)
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
    if(context.finished_ || context.known_active_reader_)
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
    known_child_ = other.known_child_;
    locally_finished_ = other.locally_finished_;
    known_origin_ = other.known_origin_;
    known_frame_start_ = other.known_frame_start_;
    if(context_ && context_->known_active_reader_ == &other) context_->known_active_reader_ = this;
    other.context_ = nullptr;
}

Result<void> BitReader::fail(Error error) noexcept {
    if(context_) {
        if(!context_->failed_) error = map_known_error(error);
        context_->fail(error);
    }
    return Result<void>::failure(context_ && context_->failed_ ? context_->error_ : error);
}

Result<void> BitReader::validate_live() const noexcept {
    if(!context_) return Result<void>::failure({ErrorCode::invalid_state, cursor_bit_});
    if(context_->failed_) return Result<void>::failure(context_->error_);
    if(context_->finished_ || locally_finished_ ||
       (context_->known_active_reader_ && context_->known_active_reader_ != this))
        return Result<void>::failure(map_known_error({ErrorCode::invalid_state, cursor_bit_}));
    return Result<void>::success();
}

Result<void> BitReader::preflight(std::size_t bit_count, std::size_t start) noexcept {
    auto live = validate_live();
    if(!live) return live;
    if(start > logical_bit_limit_ || bit_count > logical_bit_limit_ - start)
        return fail({ErrorCode::truncated_input, logical_bit_limit_});
    std::size_t projected = 0;
    if(!known_child_ && (!checked_add_size(context_->wire_bits_, bit_count, projected) ||
       projected > context_->limits_.max_wire_bits))
        return fail({ErrorCode::resource_limit, start});
    return Result<void>::success();
}

Result<bool> BitReader::read_bit() {
    auto ready = preflight(1, cursor_bit_);
    if(!ready) return Result<bool>::failure(ready.error());
    const bool value = get_bit(input_, cursor_bit_) != 0;
    ++cursor_bit_;
    charge_wire(1);
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
    charge_wire(padding);
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
    charge_wire(total);
    return Result<std::uint16_t>::success(static_cast<std::uint16_t>(accumulator));
}

Result<void> BitReader::validate_complete_value() {
    auto live = validate_live();
    if(!live) return live;
    if(cursor_bit_ == 0) {
        if(input_.empty()) return fail({ErrorCode::truncated_input, 0});
        std::size_t projected_wire = 0;
        if(!known_child_ && (!checked_add_size(context_->wire_bits_, 8, projected_wire) ||
           projected_wire > context_->limits_.max_wire_bits))
            return fail({ErrorCode::resource_limit, 0});
        for(std::size_t i = 0; i < 8; ++i)
            if(get_bit(input_, i) != 0) return fail({ErrorCode::nonzero_padding, i});
        if(input_.size() > 1) return fail({ErrorCode::trailing_data, 8});
        if(!known_child_) context_->wire_bits_ = projected_wire;
        cursor_bit_ = 8;
        if(known_child_) locally_finished_ = true;
    else context_->finished_ = true;
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
    charge_wire(padding);
    if(known_child_) locally_finished_ = true;
    else context_->finished_ = true;
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
    charge_wire(total);
    return Result<std::uint64_t>::success(value);
}

namespace {
struct BoundedIntegerLayout {
    unsigned payload_bits;
    unsigned selector_bits;
    unsigned maximum_octets;
    bool aligned;
};
BoundedIntegerLayout bounded_integer_layout(std::uint64_t maximum_offset) noexcept {
    if(maximum_offset == 0) return {0,0,0,false};
    if(maximum_offset <= 254) {
        unsigned bits=0;
        for(auto rest=maximum_offset;rest;rest>>=1) ++bits;
        return {bits,0,0,false};
    }
    if(maximum_offset == 255) return {8,0,1,true};
    if(maximum_offset <= 65535) return {16,0,2,true};
    unsigned octets=1,bits=0;
    for(auto rest=maximum_offset>>8;rest;rest>>=8) ++octets;
    for(auto rest=octets-1;rest;rest>>=1) ++bits;
    return {0,bits,octets,true};
}
std::uint64_t ordered_signed(std::int64_t value) noexcept {
    return static_cast<std::uint64_t>(value) ^ (std::uint64_t{1}<<63);
}
std::int64_t signed_ordered(std::uint64_t value) noexcept {
    const auto sign=std::uint64_t{1}<<63;
    if(value>=sign) return static_cast<std::int64_t>(value-sign);
    return std::numeric_limits<std::int64_t>::min()+static_cast<std::int64_t>(value);
}
}

Result<std::uint64_t> BitReader::read_bounded_uint(std::uint64_t lower,std::uint64_t upper) {
    auto live=validate_live();
    if(!live) return Result<std::uint64_t>::failure(live.error());
    const auto start=cursor_bit_;
    auto reject=[&](ErrorCode code,std::size_t position) {
        return Result<std::uint64_t>::failure(fail({code,position}).error());
    };
    if(lower>upper) return reject(ErrorCode::invalid_argument,start);
    const auto maximum_offset=upper-lower;
    const auto layout=bounded_integer_layout(maximum_offset);
    unsigned octets=layout.maximum_octets;
    if(layout.selector_bits) {
        if(start>logical_bit_limit_ || layout.selector_bits>logical_bit_limit_-start)
            return reject(ErrorCode::truncated_input,logical_bit_limit_);
        unsigned selector=0;
        for(unsigned i=0;i<layout.selector_bits;++i) selector=(selector<<1)|get_bit(input_,start+i);
        octets=selector+1;
        if(octets>layout.maximum_octets) return reject(ErrorCode::constraint_violation,start);
    }
    std::size_t after_selector=0,total=0;
    if(!checked_add_size(start,layout.selector_bits,after_selector)) return reject(ErrorCode::resource_limit,start);
    const auto padding=layout.aligned ? (8-after_selector%8)%8 : 0;
    const auto payload_bits=layout.selector_bits ? octets*8u : layout.payload_bits;
    if(!checked_add_size(layout.selector_bits,padding+payload_bits,total)) return reject(ErrorCode::resource_limit,start);
    auto ready=preflight(total,start);
    if(!ready) return Result<std::uint64_t>::failure(ready.error());
    for(std::size_t i=0;i<padding;++i) if(get_bit(input_,after_selector+i)) return reject(ErrorCode::nonzero_padding,after_selector+i);
    std::uint64_t offset=0;
    for(unsigned i=0;i<payload_bits;++i) offset=(offset<<1)|get_bit(input_,after_selector+padding+i);
    if((layout.selector_bits && octets>1 && (offset>>((octets-1)*8))==0) || offset>maximum_offset)
        return reject(ErrorCode::constraint_violation,start);
    cursor_bit_=start+total; charge_wire(total);
    return Result<std::uint64_t>::success(lower+offset);
}

Result<std::int64_t> BitReader::read_bounded_int(std::int64_t lower,std::int64_t upper) {
    auto live=validate_live();
    if(!live) return Result<std::int64_t>::failure(live.error());
    if(lower>upper) return Result<std::int64_t>::failure(fail({ErrorCode::invalid_argument,cursor_bit_}).error());
    auto value=read_bounded_uint(ordered_signed(lower),ordered_signed(upper));
    return value ? Result<std::int64_t>::success(signed_ordered(value.value())) : Result<std::int64_t>::failure(value.error());
}

Result<void> BitWriter::write_bounded_uint(std::uint64_t value,std::uint64_t lower,std::uint64_t upper) {
    auto live=validate_live();
    if(!live) return live;
    const auto start=cursor_bit_;
    if(lower>upper) return fail({ErrorCode::invalid_argument,start});
    if(value<lower || value>upper) return fail({ErrorCode::constraint_violation,start});
    const auto offset=value-lower;
    const auto layout=bounded_integer_layout(upper-lower);
    unsigned octets=layout.maximum_octets;
    if(layout.selector_bits) { octets=1; for(auto rest=offset>>8;rest;rest>>=8) ++octets; }
    std::size_t after_selector=0,total=0,end=0;
    if(!checked_add_size(start,layout.selector_bits,after_selector)) return fail({ErrorCode::resource_limit,start});
    const auto padding=layout.aligned ? (8-after_selector%8)%8 : 0;
    const auto payload_bits=layout.selector_bits ? octets*8u : layout.payload_bits;
    if(!checked_add_size(layout.selector_bits,padding+payload_bits,total) || !checked_add_size(start,total,end))
        return fail({ErrorCode::resource_limit,start});
    auto ready=preflight(total,end,start);
    if(!ready) return ready;
    std::size_t output_octets=0; (void)checked_bits_to_octets(end,output_octets);
    auto grown=grow_to(output_octets,start);
    if(!grown) return grown;
    for(unsigned i=0;i<layout.selector_bits;++i) set_bit(output_,start+i,(((octets-1)>>(layout.selector_bits-i-1))&1u)!=0);
    for(std::size_t i=0;i<padding;++i) set_bit(output_,after_selector+i,false);
    for(unsigned i=0;i<payload_bits;++i) set_bit(output_,after_selector+padding+i,((offset>>(payload_bits-i-1))&1u)!=0);
    cursor_bit_=end; charge_wire(total); publish_output(output_octets);
    return Result<void>::success();
}

Result<void> BitWriter::write_bounded_int(std::int64_t value,std::int64_t lower,std::int64_t upper) {
    auto live=validate_live();
    if(!live) return live;
    if(lower>upper) return fail({ErrorCode::invalid_argument,cursor_bit_});
    if(value<lower || value>upper) return fail({ErrorCode::constraint_violation,cursor_bit_});
    return write_bounded_uint(ordered_signed(value),ordered_signed(lower),ordered_signed(upper));
}

namespace {
bool integer_root_valid(std::span<const IntegerInterval> root) noexcept {
    if(root.empty()) return false;
    for(std::size_t i=0;i<root.size();++i) {
        if(root[i].lower>root[i].upper) return false;
        if(i && (root[i-1].upper==std::numeric_limits<std::int64_t>::max() || root[i].lower<=root[i-1].upper+1)) return false;
    }
    return true;
}
bool integer_root_contains(std::span<const IntegerInterval> root,std::int64_t value) noexcept {
    for(const auto& interval:root) if(value>=interval.lower && value<=interval.upper) return true;
    return false;
}
}
Result<void> BitWriter::write_extensible_int(std::int64_t value,std::int64_t lower,std::int64_t upper) {
    const IntegerInterval root{lower,upper}; return write_integer_set(value,{&root,1},true);
}
Result<std::int64_t> BitReader::read_extensible_int(std::int64_t lower,std::int64_t upper) {
    const IntegerInterval root{lower,upper}; return read_integer_set({&root,1},true);
}
Result<void> BitWriter::write_integer_set(std::int64_t value,std::span<const IntegerInterval> root,bool extensible) {
    auto live=validate_live(); if(!live) return live;
    const auto start=cursor_bit_;
    if(!integer_root_valid(root)) return fail({ErrorCode::invalid_argument,start});
    const auto lower=root.front().lower,upper=root.back().upper;
    const unsigned flag_bits=extensible ? 1u : 0u;
    const bool extension=value<lower || value>upper;
    if((extension && !extensible) || (!extension && !integer_root_contains(root,value)))
        return fail({ErrorCode::constraint_violation,start});
    const auto layout=bounded_integer_layout(ordered_signed(upper)-ordered_signed(lower));
    std::uint64_t payload=extension ? static_cast<std::uint64_t>(value) : ordered_signed(value)-ordered_signed(lower);
    unsigned octets=layout.maximum_octets;
    unsigned prefix=layout.selector_bits;
    unsigned payload_bits=layout.payload_bits;
    if(extension) {
        octets=8;
        while(octets>1) {
            const auto top=static_cast<unsigned>((payload>>((octets-1)*8))&255u);
            const auto next=static_cast<unsigned>((payload>>((octets-2)*8))&255u);
            if((top==0 && !(next&128u)) || (top==255 && (next&128u))) --octets; else break;
        }
        prefix=8; payload_bits=octets*8;
    } else if(prefix) {
        octets=1; for(auto rest=payload>>8;rest;rest>>=8) ++octets;
        payload_bits=octets*8;
    }
    std::size_t after_flag=0,after_prefix=0,total=0,end=0;
    if(!checked_add_size(start,flag_bits,after_flag)) return fail({ErrorCode::resource_limit,start});
    if(!checked_add_size(after_flag,prefix,after_prefix)) return fail({ErrorCode::resource_limit,start});
    const auto padding=extension ? (8-after_flag%8)%8 :
        layout.aligned ? (8-after_prefix%8)%8 : 0;
    if(!checked_add_size(flag_bits,padding+prefix+payload_bits,total) || !checked_add_size(start,total,end))
        return fail({ErrorCode::resource_limit,start});
    auto ready=preflight(total,end,start); if(!ready) return ready;
    std::size_t output_octets=0; (void)checked_bits_to_octets(end,output_octets);
    auto grown=grow_to(output_octets,start); if(!grown) return grown;
    if(extensible) set_bit(output_,start,extension);
    std::size_t pos=after_flag;
    if(extension) {
        for(std::size_t i=0;i<padding;++i) set_bit(output_,pos++,false);
        for(unsigned i=0;i<8;++i) set_bit(output_,pos++,((octets>>(7-i))&1u)!=0);
    } else {
        for(unsigned i=0;i<prefix;++i) set_bit(output_,pos++,(((octets-1)>>(prefix-i-1))&1u)!=0);
        for(std::size_t i=0;i<padding;++i) set_bit(output_,pos++,false);
    }
    after_prefix=pos;
    for(unsigned i=0;i<payload_bits;++i) set_bit(output_,after_prefix+i,((payload>>(payload_bits-i-1))&1u)!=0);
    cursor_bit_=end; charge_wire(total); publish_output(output_octets);
    return Result<void>::success();
}

Result<std::int64_t> BitReader::read_integer_set(std::span<const IntegerInterval> root,bool extensible) {
    auto live=validate_live(); if(!live) return Result<std::int64_t>::failure(live.error());
    const auto start=cursor_bit_;
    auto reject=[&](ErrorCode code,std::size_t offset) {
        return Result<std::int64_t>::failure(fail({code,offset}).error());
    };
    if(!integer_root_valid(root)) return reject(ErrorCode::invalid_argument,start);
    const auto lower=root.front().lower,upper=root.back().upper;
    const unsigned flag_bits=extensible ? 1u : 0u;
    if(extensible && start>=logical_bit_limit_) return reject(ErrorCode::truncated_input,logical_bit_limit_);
    const bool extension=extensible && get_bit(input_,start)!=0;
    const auto maximum=ordered_signed(upper)-ordered_signed(lower);
    const auto layout=bounded_integer_layout(maximum);
    std::size_t after_flag=0;
    if(!checked_add_size(start,flag_bits,after_flag)) return reject(ErrorCode::resource_limit,start);
    unsigned octets=layout.maximum_octets, prefix=layout.selector_bits, payload_bits=layout.payload_bits;
    std::size_t padding=0,payload_start=0,total=0;
    if(extension) {
        padding=(8-after_flag%8)%8;
        if(!checked_add_size(after_flag,padding,payload_start)) return reject(ErrorCode::resource_limit,start);
        if(payload_start>logical_bit_limit_ || logical_bit_limit_-payload_start<8)
            return reject(ErrorCode::truncated_input,logical_bit_limit_);
        unsigned length=0; for(unsigned i=0;i<8;++i) length=(length<<1)|get_bit(input_,payload_start+i);
        if(length>=192) return reject(length==192 || length>196 ? ErrorCode::constraint_violation : ErrorCode::resource_limit,start);
        if(length>=128) {
            if(logical_bit_limit_-payload_start<16) return reject(ErrorCode::truncated_input,logical_bit_limit_);
            unsigned low=0; for(unsigned i=0;i<8;++i) low=(low<<1)|get_bit(input_,payload_start+8+i);
            length=((length&63u)<<8)|low;
            return reject(length<=127 ? ErrorCode::constraint_violation : ErrorCode::resource_limit,start);
        }
        if(!length) return reject(ErrorCode::constraint_violation,start);
        if(length>8) return reject(ErrorCode::resource_limit,start);
        octets=length; prefix=8; payload_bits=octets*8; payload_start+=8;
    } else {
        if(prefix) {
            if(logical_bit_limit_-after_flag<prefix) return reject(ErrorCode::truncated_input,logical_bit_limit_);
            unsigned selector=0; for(unsigned i=0;i<prefix;++i) selector=(selector<<1)|get_bit(input_,after_flag+i);
            octets=selector+1;
            if(octets>layout.maximum_octets) return reject(ErrorCode::constraint_violation,start);
            payload_bits=octets*8;
        }
        std::size_t after_prefix=0;
        if(!checked_add_size(after_flag,prefix,after_prefix)) return reject(ErrorCode::resource_limit,start);
        padding=layout.aligned ? (8-after_prefix%8)%8 : 0;
        if(!checked_add_size(after_prefix,padding,payload_start)) return reject(ErrorCode::resource_limit,start);
    }
    if(!checked_add_size(flag_bits,padding+prefix+payload_bits,total)) return reject(ErrorCode::resource_limit,start);
    auto ready=preflight(total,start); if(!ready) return Result<std::int64_t>::failure(ready.error());
    const auto padding_start=extension ? after_flag : after_flag+prefix;
    for(std::size_t i=0;i<padding;++i) if(get_bit(input_,padding_start+i)) return reject(ErrorCode::nonzero_padding,padding_start+i);
    std::uint64_t payload=0;
    for(unsigned i=0;i<payload_bits;++i) payload=(payload<<1)|get_bit(input_,payload_start+i);
    std::int64_t value=0;
    if(extension) {
        const auto top=static_cast<unsigned>((payload>>((octets-1)*8))&255u);
        if(octets>1) {
            const auto next=static_cast<unsigned>((payload>>((octets-2)*8))&255u);
            if((top==0 && !(next&128u)) || (top==255 && (next&128u))) return reject(ErrorCode::constraint_violation,start);
        }
        if(top&128u) {
            if(payload_bits<64) payload|=(~std::uint64_t{0})<<payload_bits;
            value=signed_ordered(payload^(std::uint64_t{1}<<63));
        } else value=static_cast<std::int64_t>(payload);
        if(value>=lower && value<=upper) return reject(ErrorCode::constraint_violation,start);
    } else {
        if(payload>maximum || (prefix && octets>1 && (payload>>((octets-1)*8))==0)) return reject(ErrorCode::constraint_violation,start);
        value=signed_ordered(ordered_signed(lower)+payload);
        if(!integer_root_contains(root,value)) return reject(ErrorCode::constraint_violation,start);
    }
    cursor_bit_=start+total; charge_wire(total);
    return Result<std::int64_t>::success(value);
}

BitWriter::BitWriter(BitWriter&& other) noexcept
    : context_(other.context_), output_(std::move(other.output_)), cursor_bit_(other.cursor_bit_) {
    known_child_ = other.known_child_;
    locally_finished_ = other.locally_finished_;
    known_encode_anchor_ = other.known_encode_anchor_;
    known_staged_octets_ = other.known_staged_octets_;
    other.known_staged_octets_ = 0;
    if(context_ && context_->known_active_writer_ == &other) context_->known_active_writer_ = this;
    other.context_ = nullptr;
    other.cursor_bit_ = 0;
}

Result<void> BitWriter::fail(Error error) noexcept {
    if(context_) {
        if(!context_->failed_ && known_child_) error.bit_offset = known_encode_anchor_;
        context_->fail(error);
    }
    return Result<void>::failure(context_ && context_->failed_ ? context_->error_ : error);
}

Result<void> BitWriter::validate_live() const noexcept {
    if(!context_) return Result<void>::failure({ErrorCode::invalid_state, cursor_bit_});
    if(context_->failed_) return Result<void>::failure(context_->error_);
    if(context_->finished_ || locally_finished_ ||
       (context_->known_active_writer_ && context_->known_active_writer_ != this))
        return Result<void>::failure({ErrorCode::invalid_state, known_child_ ? known_encode_anchor_ : cursor_bit_});
    return Result<void>::success();
}

Result<void> BitWriter::preflight(std::size_t bit_count, std::size_t projected_end,
                                  std::size_t start) noexcept {
    auto live = validate_live();
    if(!live) return live;
    if(known_child_) {
        std::size_t octets = 0, projected = 0;
        (void)checked_bits_to_octets(projected_end, octets);
        const auto extra = octets > output_.size() ? octets - output_.size() : 0;
        if(octets > output_.max_size() ||
           !checked_add_size(context_->known_open_staging_octets_, extra, projected) ||
           projected > context_->limits_.max_known_open_staging_octets)
            return fail({ErrorCode::resource_limit, start});
        return Result<void>::success();
    }
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
    const auto extra = octets - output_.size();
    if(known_child_) {
        std::size_t projected = 0;
        if(!checked_add_size(context_->known_open_staging_octets_, extra, projected) ||
           projected > context_->limits_.max_known_open_staging_octets)
            return fail({ErrorCode::resource_limit, start});
        context_->known_open_staging_octets_ = projected;
    }
    try {
        output_.resize(octets, std::byte{0});
    } catch(const std::bad_alloc&) {
        if(known_child_) context_->known_open_staging_octets_ -= extra;
        return fail({ErrorCode::allocation_failure, start});
    } catch(const std::length_error&) {
        if(known_child_) context_->known_open_staging_octets_ -= extra;
        return fail({ErrorCode::resource_limit, start});
    }
    if(known_child_) known_staged_octets_ += extra;
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
    charge_wire(1);
    publish_output(octets);
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
    charge_wire(padding);
    publish_output(octets);
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
    charge_wire(total);
    publish_output(octets);
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
    charge_wire(padding);
    publish_output(octets);
    if(known_child_) locally_finished_ = true;
    else context_->finished_ = true;
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
    charge_wire(total);
    publish_output(output_octets);
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
    charge_wire(total);
    publish_output(output_octets);
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
    charge_wire(end - start);
    return Result<EnumeratedIndex>::success(value);
}

namespace {

enum class FramingPass { availability, padding, canonical, copy };
struct FramingScan { std::size_t end; std::size_t units; };

// Re-scan bounded input instead of retaining attacker-sized fragment descriptors.
// Availability and size budgets precede padding and canonicality checks.
Result<FramingScan> scan_extension_frame(std::span<const std::byte> input,
                                        std::size_t limit, std::size_t start,
                                        bool bitmap, FramingPass pass,
                                        std::size_t expected_units,
                                        std::vector<std::byte>* output = nullptr) {
    std::size_t position = start;
    std::size_t total = 0;
    bool any_present = false;
    auto reject = [](ErrorCode code, std::size_t offset) {
        return Result<FramingScan>::failure({code, offset});
    };
    auto take = [&](unsigned bits, std::size_t& value) {
        if(position > limit || bits > limit - position) return false;
        value = 0;
        for(unsigned i = 0; i < bits; ++i)
            value = (value << 1) | get_bit(input, position++);
        return true;
    };
    auto align = [&]() -> Result<void> {
        const std::size_t padding = (8 - position % 8) % 8;
        if(position > limit || padding > limit - position)
            return Result<void>::failure({ErrorCode::truncated_input, limit});
        if(pass == FramingPass::padding) {
            for(std::size_t i = 0; i < padding; ++i)
                if(get_bit(input, position + i))
                    return Result<void>::failure({ErrorCode::nonzero_padding, position + i});
        }
        position += padding;
        return Result<void>::success();
    };
    auto contents = [&](std::size_t units) -> Result<void> {
        std::size_t bits = units;
        std::size_t next_total = 0;
        if((!bitmap && !checked_octets_to_bits(units, bits)) ||
           !checked_add_size(total, units, next_total))
            return Result<void>::failure({ErrorCode::resource_limit, start});
        if(position > limit || bits > limit - position)
            return Result<void>::failure({ErrorCode::truncated_input, limit});
        if(bitmap && (pass == FramingPass::canonical || pass == FramingPass::copy)) {
            for(std::size_t i = 0; i < units; ++i) {
                const bool value = get_bit(input, position + i) != 0;
                any_present = any_present || value;
                if(output) set_bit(*output, total + i, value);
            }
        } else if(output) {
            for(std::size_t i = 0; i < units; ++i)
                (*output)[total + i] = input[position / 8 + i];
        }
        position += bits;
        total = next_total;
        return Result<void>::success();
    };
    if(bitmap) {
        std::size_t large = 0;
        if(!take(1, large)) return reject(ErrorCode::truncated_input, limit);
        if(large == 0) {
            std::size_t count_minus_one = 0;
            if(!take(6, count_minus_one)) return reject(ErrorCode::truncated_input, limit);
            auto added = contents(count_minus_one + 1);
            if(!added) return reject(added.error().code, added.error().bit_offset);
            if(pass == FramingPass::canonical && !any_present)
                return reject(ErrorCode::constraint_violation, start);
            return Result<FramingScan>::success({position, total});
        }
    }
    for(;;) {
        auto aligned = align();
        if(!aligned) return reject(aligned.error().code, aligned.error().bit_offset);
        const std::size_t determinant = position;
        std::size_t first = 0;
        if(!take(8, first)) return reject(ErrorCode::truncated_input, limit);
        std::size_t units = 0;
        bool fragmented = false;
        bool overlong = false;
        if((first & 0x80u) == 0) {
            units = first;
        } else if((first & 0xc0u) == 0x80u) {
            std::size_t low = 0;
            if(!take(8, low)) return reject(ErrorCode::truncated_input, limit);
            units = ((first & 0x3fu) << 8) | low;
            overlong = units < 128;
        } else {
            const std::size_t multiplier = first & 0x3fu;
            if(multiplier < 1 || multiplier > 4)
                return reject(ErrorCode::constraint_violation, determinant);
            units = multiplier * 16384;
            fragmented = true;
        }
        if(pass == FramingPass::canonical) {
            if(overlong) return reject(ErrorCode::constraint_violation, determinant);
            if(fragmented) {
                const std::size_t remaining = expected_units - total;
                const std::size_t maximum = remaining / 16384 < 4 ? remaining / 16384 : 4;
                if(units / 16384 != maximum)
                    return reject(ErrorCode::constraint_violation, determinant);
            }
        }
        auto added = contents(units);
        if(!added) return reject(added.error().code, added.error().bit_offset);
        if(!fragmented) break;
    }
    if(pass == FramingPass::canonical &&
       (total == 0 || (bitmap && (total <= 64 || !any_present))))
        return reject(ErrorCode::constraint_violation, start);
    return Result<FramingScan>::success({position, total});
}

} // namespace

Result<SequenceExtensionBitmap> BitReader::read_sequence_extension_bitmap() {
    auto live = validate_live();
    if(!live) return Result<SequenceExtensionBitmap>::failure(live.error());
    const std::size_t start = cursor_bit_;
    auto reject = [&](Error error) {
        return Result<SequenceExtensionBitmap>::failure(fail(error).error());
    };
    auto scanned = scan_extension_frame(input_, logical_bit_limit_, start, true,
                                         FramingPass::availability, 0);
    if(!scanned) return reject(scanned.error());
    const auto frame = scanned.value();
    std::size_t projected = 0;
    if(!checked_add_size(context_->extension_bitmap_bits_, frame.units, projected) ||
       projected > context_->limits_.max_extension_bitmap_bits)
        return reject({ErrorCode::resource_limit, start});
    auto ready = preflight(frame.end - start, start);
    if(!ready) return Result<SequenceExtensionBitmap>::failure(ready.error());
    for(auto pass : {FramingPass::padding, FramingPass::canonical}) {
        auto checked = scan_extension_frame(input_, logical_bit_limit_, start, true, pass, frame.units);
        if(!checked) return reject(checked.error());
    }
    SequenceExtensionBitmap result{frame.units, {}};
    std::size_t octets = 0;
    (void)checked_bits_to_octets(frame.units, octets);
    if(octets > result.packed_bits.max_size()) return reject({ErrorCode::resource_limit, start});
    try {
        result.packed_bits.resize(octets);
    } catch(const std::bad_alloc&) {
        return reject({ErrorCode::allocation_failure, start});
    } catch(const std::length_error&) {
        return reject({ErrorCode::resource_limit, start});
    }
    auto copied = scan_extension_frame(input_, logical_bit_limit_, start, true,
                                        FramingPass::copy, frame.units, &result.packed_bits);
    if(!copied) return reject(copied.error());
    cursor_bit_ = frame.end;
    charge_wire(frame.end - start);
    context_->extension_bitmap_bits_ = projected;
    return Result<SequenceExtensionBitmap>::success(std::move(result));
}

Result<std::vector<std::byte>> BitReader::read_open_type_owned() {
    auto live = validate_live();
    if(!live) return Result<std::vector<std::byte>>::failure(live.error());
    const std::size_t start = cursor_bit_;
    auto reject = [&](Error error) {
        return Result<std::vector<std::byte>>::failure(fail(error).error());
    };
    auto scanned = scan_extension_frame(input_, logical_bit_limit_, start, false,
                                         FramingPass::availability, 0);
    if(!scanned) return reject(scanned.error());
    const auto frame = scanned.value();
    std::size_t projected_octets = 0;
    std::size_t projected_records = 0;
    if(!checked_add_size(context_->retained_unknown_payload_octets_, frame.units, projected_octets) ||
       projected_octets > context_->limits_.max_retained_unknown_payload_octets ||
       !checked_add_size(context_->retained_unknown_records_, 1, projected_records) ||
       projected_records > context_->limits_.max_retained_unknown_records)
        return reject({ErrorCode::resource_limit, start});
    auto ready = preflight(frame.end - start, start);
    if(!ready) return Result<std::vector<std::byte>>::failure(ready.error());
    for(auto pass : {FramingPass::padding, FramingPass::canonical}) {
        auto checked = scan_extension_frame(input_, logical_bit_limit_, start, false, pass, frame.units);
        if(!checked) return reject(checked.error());
    }
    std::vector<std::byte> result;
    if(frame.units > result.max_size()) return reject({ErrorCode::resource_limit, start});
    try {
        result.resize(frame.units);
    } catch(const std::bad_alloc&) {
        return reject({ErrorCode::allocation_failure, start});
    } catch(const std::length_error&) {
        return reject({ErrorCode::resource_limit, start});
    }
    auto copied = scan_extension_frame(input_, logical_bit_limit_, start, false,
                                        FramingPass::copy, frame.units, &result);
    if(!copied) return reject(copied.error());
    cursor_bit_ = frame.end;
    charge_wire(frame.end - start);
    context_->retained_unknown_payload_octets_ = projected_octets;
    context_->retained_unknown_records_ = projected_records;
    return Result<std::vector<std::byte>>::success(std::move(result));
}

Result<void> BitWriter::reject_sequence_extension_data() {
    auto live = validate_live();
    if(!live) return live;
    return fail({ErrorCode::constraint_violation, cursor_bit_});
}

// X.691 11.9: aligned unconstrained length in element units. Each segment
// atomically validates/charges only its determinant and declared element count;
// element payloads remain ordinary composite operations on the shared context.
Result<CollectionLengthSegment> BitReader::read_collection_segment() {
    auto live = validate_live();
    if(!live) return Result<CollectionLengthSegment>::failure(live.error());
    const auto start = cursor_bit_;
    auto reject = [&](Error e) { return Result<CollectionLengthSegment>::failure(fail(e).error()); };
    const auto padding = (8 - start % 8) % 8;
    auto ready = preflight(padding + 8, start);
    if(!ready) return Result<CollectionLengthSegment>::failure(ready.error());
    for(std::size_t i = 0; i < padding; ++i)
        if(get_bit(input_, start+i)) return reject({ErrorCode::nonzero_padding, start+i});
    unsigned first = 0;
    for(unsigned i=0; i<8; ++i) first = (first << 1) | get_bit(input_,start+padding+i);
    std::size_t count = first, width = 8;
    bool fragmented = false;
    if(first >= 192) {
        const auto units = first & 63u;
        if(units == 0 || units > 4) return reject({ErrorCode::constraint_violation,start});
        count = static_cast<std::size_t>(units) * 16384; fragmented = true;
    } else if(first >= 128) {
        ready = preflight(padding+16,start);
        if(!ready) return Result<CollectionLengthSegment>::failure(ready.error());
        unsigned second = 0;
        for(unsigned i=0;i<8;++i) second = (second << 1) | get_bit(input_,start+padding+8+i);
        count = static_cast<std::size_t>(first & 63u)*256 + second; width=16;
        if(count < 128) return reject({ErrorCode::constraint_violation,start});
    }
    std::size_t elements = 0;
    if(!checked_add_size(context_->collection_elements_,count,elements) || elements > context_->limits_.max_collection_elements)
        return reject({ErrorCode::resource_limit,start});
    cursor_bit_ = start+padding+width;
    charge_wire(padding+width); context_->collection_elements_=elements;
    return Result<CollectionLengthSegment>::success({count,fragmented});
}
Result<void> BitWriter::write_collection_segment(std::size_t count, bool fragmented) {
    auto live=validate_live(); if(!live) return live;
    const auto start=cursor_bit_;
    if((fragmented && (count < 16384 || count > 65536 || count % 16384)) || (!fragmented && count >= 16384))
        return fail({ErrorCode::invalid_argument,start});
    std::size_t elements=0;
    if(!checked_add_size(context_->collection_elements_,count,elements) || elements > context_->limits_.max_collection_elements)
        return fail({ErrorCode::resource_limit,start});
    const auto padding=(8-start%8)%8;
    const std::size_t width=fragmented || count < 128 ? 8 : 16;
    std::size_t end=0;
    if(!checked_add_size(start,padding+width,end)) return fail({ErrorCode::resource_limit,start});
    auto ready=preflight(padding+width,end,start); if(!ready) return ready;
    std::size_t octets=0; (void)checked_bits_to_octets(end,octets);
    auto grown=grow_to(octets,start); if(!grown) return grown;
    const auto determinant=fragmented ? 192u+static_cast<unsigned>(count/16384) :
        width==8 ? static_cast<unsigned>(count) : 32768u+static_cast<unsigned>(count);
    for(std::size_t i=0;i<padding;++i) set_bit(output_,start+i,false);
    for(unsigned i=0;i<width;++i) set_bit(output_,start+padding+i,((determinant >> (width-i-1))&1u)!=0);
    cursor_bit_=end; charge_wire(padding+width); publish_output(octets); context_->collection_elements_=elements;
    return Result<void>::success();
}

// The determinant is a constrained whole-number offset, not a general
// unconstrained/fragmented length. Fixed lengths still consume element budget.
Result<std::size_t> BitReader::read_bounded_collection_length(std::size_t lower,
                                                             std::size_t upper) {
    auto live = validate_live();
    if(!live) return Result<std::size_t>::failure(live.error());
    const auto start = cursor_bit_;
    auto reject = [&](ErrorCode code, std::size_t offset) {
        return Result<std::size_t>::failure(fail({code, offset}).error());
    };
    if(lower > upper || upper > 65535) return reject(ErrorCode::invalid_argument, start);
    const auto range = static_cast<std::uint64_t>(upper - lower) + 1;
    unsigned width = 0;
    if(range >= 257) width = 16;
    else if(range == 256) width = 8;
    else for(auto remaining = range - 1; remaining; remaining >>= 1) ++width;
    const auto padding = range >= 256 ? (8 - start % 8) % 8 : 0;
    const auto total = padding + width;
    auto ready = preflight(total, start);
    if(!ready) return Result<std::size_t>::failure(ready.error());
    for(std::size_t i = 0; i < padding; ++i)
        if(get_bit(input_, start + i)) return reject(ErrorCode::nonzero_padding, start + i);
    std::uint64_t offset = 0;
    for(unsigned i = 0; i < width; ++i)
        offset = (offset << 1) | get_bit(input_, start + padding + i);
    if(offset >= range) return reject(ErrorCode::constraint_violation, start);
    const auto count = lower + static_cast<std::size_t>(offset);
    std::size_t elements = 0;
    if(!checked_add_size(context_->collection_elements_, count, elements) ||
       elements > context_->limits_.max_collection_elements)
        return reject(ErrorCode::resource_limit, start);
    cursor_bit_ = start + total;
    charge_wire(total);
    context_->collection_elements_ = elements;
    return Result<std::size_t>::success(count);
}

Result<void> BitWriter::write_bounded_collection_length(std::uint64_t count,
                                                       std::size_t lower,
                                                       std::size_t upper) {
    auto live = validate_live();
    if(!live) return live;
    const auto start = cursor_bit_;
    if(lower > upper || upper > 65535) return fail({ErrorCode::invalid_argument, start});
    if(count < lower || count > upper) return fail({ErrorCode::constraint_violation, start});
    std::size_t elements = 0;
    if(!checked_add_size(context_->collection_elements_, static_cast<std::size_t>(count), elements) ||
       elements > context_->limits_.max_collection_elements)
        return fail({ErrorCode::resource_limit, start});
    const auto range = static_cast<std::uint64_t>(upper - lower) + 1;
    unsigned width = 0;
    if(range >= 257) width = 16;
    else if(range == 256) width = 8;
    else for(auto remaining = range - 1; remaining; remaining >>= 1) ++width;
    const auto padding = range >= 256 ? (8 - start % 8) % 8 : 0;
    const auto total = padding + width;
    std::size_t end = 0;
    if(!checked_add_size(start, total, end)) return fail({ErrorCode::resource_limit, start});
    auto ready = preflight(total, end, start);
    if(!ready) return ready;
    std::size_t octets = 0;
    (void)checked_bits_to_octets(end, octets);
    auto grown = grow_to(octets, start);
    if(!grown) return grown;
    for(std::size_t i = 0; i < padding; ++i) set_bit(output_, start + i, false);
    const auto offset = count - lower;
    for(unsigned i = 0; i < width; ++i)
        set_bit(output_, start + padding + i, ((offset >> (width - i - 1)) & 1u) != 0);
    cursor_bit_ = end;
    charge_wire(total);
    publish_output(octets);
    context_->collection_elements_ = elements;
    return Result<void>::success();
}

namespace {
unsigned octet_length_width(std::size_t lower, std::size_t upper) noexcept {
    const auto range = upper - lower + 1;
    if(range >= 257) return 16;
    if(range == 256) return 8;
    unsigned width = 0;
    for(auto remaining = range - 1; remaining; remaining >>= 1) ++width;
    return width;
}
}

Result<std::vector<std::byte>> BitReader::read_octet_string_owned(
    std::size_t lower, std::size_t upper, bool unconstrained, bool extensible) {
    auto live = validate_live();
    if(!live) return Result<std::vector<std::byte>>::failure(live.error());
    const auto start = cursor_bit_;
    auto reject = [&](ErrorCode code, std::size_t offset) {
        return Result<std::vector<std::byte>>::failure(fail({code,offset}).error());
    };
    if((unconstrained && (lower || upper || extensible)) || (!unconstrained && (lower > upper || upper > 65535)))
        return reject(ErrorCode::invalid_argument,start);
    const auto root_lower = lower, root_upper = upper;
    bool extension = false;
    if(extensible) {
        if(start >= logical_bit_limit_) return reject(ErrorCode::truncated_input,logical_bit_limit_);
        extension = get_bit(input_,start) != 0;
        if(extension) { unconstrained = true; lower = upper = 0; }
    }
    const auto content_start = start + static_cast<std::size_t>(extensible);
    const bool variable = unconstrained || lower != upper;
    const auto range = unconstrained ? std::size_t{0} : upper - lower + 1;
    const auto initial_padding = unconstrained || (!variable && upper >= 3) ||
        (variable && range >= 256) ? (8 - content_start % 8) % 8 : 0;
    std::size_t position = 0;
    if(!checked_add_size(content_start,initial_padding,position)) return reject(ErrorCode::resource_limit,start);
    unsigned width = variable ? (unconstrained ? 8 : octet_length_width(lower,upper)) : 0;
    auto available = [&](std::size_t bits) {
        return position <= logical_bit_limit_ && bits <= logical_bit_limit_ - position;
    };
    if(!available(width)) return reject(ErrorCode::truncated_input,logical_bit_limit_);
    std::size_t encoded_length = 0;
    for(unsigned i = 0; i < width; ++i)
        encoded_length = (encoded_length << 1) | get_bit(input_,position + i);
    position += width;
    bool noncanonical = false;
    if(unconstrained) {
        if(encoded_length >= 192) return reject(ErrorCode::resource_limit,start);
        if(encoded_length >= 128) {
            if(!available(8)) return reject(ErrorCode::truncated_input,logical_bit_limit_);
            encoded_length = ((encoded_length & 63u) << 8);
            for(unsigned i = 0; i < 8; ++i) encoded_length |= static_cast<std::size_t>(get_bit(input_,position + i)) << (7 - i);
            position += 8;
            noncanonical = encoded_length < 128;
        }
    } else if(variable && encoded_length >= range) {
        return reject(ErrorCode::constraint_violation,start);
    }
    const auto count = unconstrained ? encoded_length : lower + encoded_length;
    if(extension && count >= root_lower && count <= root_upper) return reject(ErrorCode::constraint_violation,start);
    const auto payload_padding = variable ? (8 - position % 8) % 8 : 0;
    std::size_t payload_bits = 0,end = 0;
    if(!checked_octets_to_bits(count,payload_bits) ||
       !checked_add_size(position,payload_padding,end) || !checked_add_size(end,payload_bits,end))
        return reject(ErrorCode::resource_limit,start);
    auto ready = preflight(end - start,start);
    if(!ready) return Result<std::vector<std::byte>>::failure(ready.error());
    for(std::size_t i = 0; i < initial_padding; ++i)
        if(get_bit(input_,content_start + i)) return reject(ErrorCode::nonzero_padding,content_start + i);
    for(std::size_t i = 0; i < payload_padding; ++i)
        if(get_bit(input_,position + i)) return reject(ErrorCode::nonzero_padding,position + i);
    if(noncanonical) return reject(ErrorCode::constraint_violation,start);
    std::vector<std::byte> value;
    if(count > value.max_size()) return reject(ErrorCode::resource_limit,start);
    try { value.resize(count); }
    catch(const std::bad_alloc&) { return reject(ErrorCode::allocation_failure,start); }
    catch(const std::length_error&) { return reject(ErrorCode::resource_limit,start); }
    const auto payload_start = position + payload_padding;
    for(std::size_t i = 0; i < count; ++i) {
        unsigned octet = 0;
        for(unsigned bit = 0; bit < 8; ++bit) octet = (octet << 1) | get_bit(input_,payload_start + i * 8 + bit);
        value[i] = static_cast<std::byte>(octet);
    }
    cursor_bit_ = end; charge_wire(end - start);
    return Result<std::vector<std::byte>>::success(std::move(value));
}

Result<void> BitWriter::write_octet_string(std::span<const std::byte> value,
    std::size_t lower, std::size_t upper, bool unconstrained, bool extensible) {
    auto live = validate_live();
    if(!live) return live;
    const auto start = cursor_bit_;
    if((unconstrained && (lower || upper || extensible)) || (!unconstrained && (lower > upper || upper > 65535))) return fail({ErrorCode::invalid_argument,start});
    const bool extension = extensible && (value.size() < lower || value.size() > upper);
    if(extension) { unconstrained = true; lower = upper = 0; }
    std::size_t content_start = 0;
    if(!checked_add_size(start, static_cast<std::size_t>(extensible), content_start)) return fail({ErrorCode::resource_limit,start});
    if(unconstrained && value.size() > 16383) return fail({ErrorCode::resource_limit,start});
    if(!unconstrained && (value.size() < lower || value.size() > upper)) return fail({ErrorCode::constraint_violation,start});
    const bool variable = unconstrained || lower != upper;
    const auto range = unconstrained ? std::size_t{0} : upper - lower + 1;
    const auto initial_padding = unconstrained || (!variable && upper >= 3) ||
        (variable && range >= 256) ? (8 - content_start % 8) % 8 : 0;
    const auto width = variable ? (unconstrained ? (value.size() < 128 ? 8u : 16u) : octet_length_width(lower,upper)) : 0u;
    std::size_t position = 0,end = 0,payload_bits = 0;
    if(!checked_add_size(content_start,initial_padding + width,position)) return fail({ErrorCode::resource_limit,start});
    const auto payload_padding = variable ? (8 - position % 8) % 8 : 0;
    if(!checked_octets_to_bits(value.size(),payload_bits) || !checked_add_size(position,payload_padding,end) ||
       !checked_add_size(end,payload_bits,end)) return fail({ErrorCode::resource_limit,start});
    auto ready = preflight(end - start,end,start);
    if(!ready) return ready;
    std::size_t octets = 0; (void)checked_bits_to_octets(end,octets);
    auto grown = grow_to(octets,start);
    if(!grown) return grown;
    if(extensible) set_bit(output_,start,extension);
    for(std::size_t i = 0; i < initial_padding; ++i) set_bit(output_,content_start + i,false);
    const auto determinant = unconstrained ? value.size() | (width == 16 ? 0x8000u : 0u) : value.size() - lower;
    for(unsigned i = 0; i < width; ++i) set_bit(output_,content_start + initial_padding + i,((determinant >> (width - i - 1)) & 1u) != 0);
    for(std::size_t i = 0; i < payload_padding; ++i) set_bit(output_,position + i,false);
    const auto payload_start = position + payload_padding;
    for(std::size_t i = 0; i < value.size(); ++i) for(unsigned bit = 0; bit < 8; ++bit)
        set_bit(output_,payload_start + i * 8 + bit,(std::to_integer<unsigned>(value[i]) & (0x80u >> bit)) != 0);
    cursor_bit_ = end; charge_wire(end - start); publish_output(octets);
    return Result<void>::success();
}

namespace {
bool printable_character(unsigned value) noexcept {
    constexpr std::string_view alphabet = " '()+,-./0123456789:=?ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    return value <= 127 && alphabet.find(static_cast<char>(value)) != std::string_view::npos;
}
bool utf8_scalar_count(std::string_view value, std::size_t& count) noexcept {
    count=0;
    for(std::size_t i=0;i<value.size();) {
        const auto first=static_cast<unsigned char>(value[i++]);
        unsigned remaining=0,scalar=0,minimum=0;
        if(first<0x80) { ++count; continue; }
        if(first>=0xC2 && first<=0xDF) { remaining=1; scalar=first&31u; minimum=0x80; }
        else if(first>=0xE0 && first<=0xEF) { remaining=2; scalar=first&15u; minimum=0x800; }
        else if(first>=0xF0 && first<=0xF4) { remaining=3; scalar=first&7u; minimum=0x10000; }
        else return false;
        if(remaining>value.size()-i) return false;
        for(unsigned j=0;j<remaining;++j) {
            const auto next=static_cast<unsigned char>(value[i++]);
            if((next&0xC0u)!=0x80u) return false;
            scalar=(scalar<<6)|(next&63u);
        }
        if(scalar<minimum || scalar>0x10FFFF || (scalar>=0xD800 && scalar<=0xDFFF)) return false;
        ++count;
    }
    return true;
}
bool character_valid(unsigned value, CharacterStringKind kind) noexcept {
    return kind==CharacterStringKind::visible ? value>=32 && value<=126 : printable_character(value);
}
}
Result<void> BitWriter::write_character_string(std::string_view value,
    std::size_t lower, std::size_t upper, bool extensible, CharacterStringKind kind, bool unconstrained) {
    auto live = validate_live();
    if(!live) return live;
    const auto start = cursor_bit_;
    if((unconstrained && (lower || upper || extensible)) || lower > upper || upper > 65535) return fail({ErrorCode::invalid_argument,start});
    if(kind!=CharacterStringKind::printable && kind!=CharacterStringKind::visible && kind!=CharacterStringKind::utf8) return fail({ErrorCode::invalid_argument,start});
    if(kind==CharacterStringKind::utf8) {
        std::size_t count=0;
        if(!utf8_scalar_count(value,count) || (!unconstrained && !extensible && (count<lower || count>upper))) return fail({ErrorCode::constraint_violation,start});
        return write_octet_string(std::as_bytes(std::span<const char>(value.data(),value.size())),0,0,true);
    }
    const bool extension = value.size() < lower || value.size() > upper;
    const bool unbounded = unconstrained || extension;
    if(extension && !extensible && !unconstrained) return fail({ErrorCode::constraint_violation,start});
    if(unbounded && value.size() >= 16384) return fail({ErrorCode::resource_limit,start});
    for(char character : value)
        if(!character_valid(static_cast<unsigned char>(character),kind)) return fail({ErrorCode::constraint_violation,start});
    std::size_t position = 0;
    if(!checked_add_size(start,extensible ? 1u : 0u,position)) return fail({ErrorCode::resource_limit,start});
    const bool variable = unbounded || lower != upper;
    const auto initial_padding = unbounded || (variable && upper - lower + 1 >= 256) ? (8-position%8)%8 : 0;
    const unsigned width = unbounded ? (value.size()<128 ? 8u : 16u) : variable ? octet_length_width(lower,upper) : 0u;
    const auto length_start = position + initial_padding;
    if(!checked_add_size(position,initial_padding + width,position)) return fail({ErrorCode::resource_limit,start});
    const bool aligned_payload = unbounded || (variable ? upper >= 2 : upper > 2);
    const auto payload_padding = !value.empty() && aligned_payload ? (8-position%8)%8 : 0;
    std::size_t payload_bits = 0,end = 0;
    if(!checked_octets_to_bits(value.size(),payload_bits) || !checked_add_size(position,payload_padding,end) ||
       !checked_add_size(end,payload_bits,end)) return fail({ErrorCode::resource_limit,start});
    auto ready = preflight(end-start,end,start); if(!ready) return ready;
    std::size_t octets = 0; (void)checked_bits_to_octets(end,octets);
    auto grown = grow_to(octets,start); if(!grown) return grown;
    if(extensible) set_bit(output_,start,extension);
    for(std::size_t i=0;i<initial_padding;++i) set_bit(output_,start+(extensible?1u:0u)+i,false);
    const auto determinant = unbounded ? value.size() | (width==16 ? 0x8000u : 0u) : value.size()-lower;
    for(unsigned i=0;i<width;++i) set_bit(output_,length_start+i,((determinant>>(width-i-1))&1u)!=0);
    for(std::size_t i=0;i<payload_padding;++i) set_bit(output_,position+i,false);
    for(std::size_t i=0;i<value.size();++i) for(unsigned bit=0;bit<8;++bit)
        set_bit(output_,position+payload_padding+i*8+bit,(static_cast<unsigned char>(value[i])&(0x80u>>bit))!=0);
    cursor_bit_=end; charge_wire(end-start); publish_output(octets);
    return Result<void>::success();
}
Result<std::string> BitReader::read_character_string_owned(std::size_t lower,
    std::size_t upper, bool extensible, CharacterStringKind kind, bool unconstrained) {
    auto live=validate_live(); if(!live) return Result<std::string>::failure(live.error());
    const auto start=cursor_bit_;
    auto reject=[&](ErrorCode code,std::size_t offset) { return Result<std::string>::failure(fail({code,offset}).error()); };
    if((unconstrained && (lower || upper || extensible)) || lower>upper || upper>65535) return reject(ErrorCode::invalid_argument,start);
    if(kind!=CharacterStringKind::printable && kind!=CharacterStringKind::visible && kind!=CharacterStringKind::utf8) return reject(ErrorCode::invalid_argument,start);
    if(kind==CharacterStringKind::utf8) {
        const auto saved_wire=context_->wire_bits_;
        auto bytes=read_octet_string_owned(0,0,true);
        if(!bytes) return Result<std::string>::failure(bytes.error());
        std::string value;
        try { if(!bytes.value().empty()) value.assign(reinterpret_cast<const char*>(bytes.value().data()),bytes.value().size()); }
        catch(const std::bad_alloc&) { cursor_bit_=start; context_->wire_bits_=saved_wire; return reject(ErrorCode::allocation_failure,start); }
        catch(const std::length_error&) { cursor_bit_=start; context_->wire_bits_=saved_wire; return reject(ErrorCode::resource_limit,start); }
        std::size_t count=0;
        if(!utf8_scalar_count(value,count) || (!unconstrained && !extensible && (count<lower || count>upper))) {
            cursor_bit_=start; context_->wire_bits_=saved_wire;
            return reject(ErrorCode::constraint_violation,start);
        }
        return Result<std::string>::success(std::move(value));
    }
    if(extensible && start>=logical_bit_limit_) return reject(ErrorCode::truncated_input,logical_bit_limit_);
    const bool extension=extensible && get_bit(input_,start);
    const bool unbounded=unconstrained || extension;
    std::size_t position=0;
    if(!checked_add_size(start,extensible?1u:0u,position)) return reject(ErrorCode::resource_limit,start);
    const bool variable=unbounded || lower!=upper;
    const auto initial_padding=unbounded || (variable && upper-lower+1>=256) ? (8-position%8)%8 : 0;
    const auto padding_start=position;
    if(!checked_add_size(position,initial_padding,position)) return reject(ErrorCode::resource_limit,start);
    auto available=[&](std::size_t count) { return position<=logical_bit_limit_ && count<=logical_bit_limit_-position; };
    unsigned width=unbounded?8u:variable?octet_length_width(lower,upper):0u;
    if(!available(width)) return reject(ErrorCode::truncated_input,logical_bit_limit_);
    std::size_t determinant=0;
    for(unsigned i=0;i<width;++i) determinant=(determinant<<1)|get_bit(input_,position+i);
    position+=width;
    bool noncanonical=false;
    if(unbounded) {
        if(determinant>=192) return reject(ErrorCode::resource_limit,start);
        if(determinant>=128) {
            if(!available(8)) return reject(ErrorCode::truncated_input,logical_bit_limit_);
            determinant=(determinant&63u)<<8;
            for(unsigned i=0;i<8;++i) determinant|=static_cast<std::size_t>(get_bit(input_,position+i))<<(7-i);
            position+=8; noncanonical=determinant<128;
        }
    } else if(variable && determinant>=upper-lower+1) return reject(ErrorCode::constraint_violation,start);
    const auto count=unbounded?determinant:lower+determinant;
    if(extension && count>=lower && count<=upper) noncanonical=true;
    const bool aligned_payload=unbounded || (variable?upper>=2:upper>2);
    const auto payload_padding=count && aligned_payload?(8-position%8)%8:0;
    std::size_t payload_bits=0,end=0;
    if(!checked_octets_to_bits(count,payload_bits) || !checked_add_size(position,payload_padding,end) ||
       !checked_add_size(end,payload_bits,end)) return reject(ErrorCode::resource_limit,start);
    auto ready=preflight(end-start,start); if(!ready) return Result<std::string>::failure(ready.error());
    for(std::size_t i=0;i<initial_padding;++i) if(get_bit(input_,padding_start+i)) return reject(ErrorCode::nonzero_padding,padding_start+i);
    for(std::size_t i=0;i<payload_padding;++i) if(get_bit(input_,position+i)) return reject(ErrorCode::nonzero_padding,position+i);
    if(noncanonical) return reject(ErrorCode::constraint_violation,start);
    std::string value;
    if(count>value.max_size()) return reject(ErrorCode::resource_limit,start);
    try { value.resize(count); }
    catch(const std::bad_alloc&) { return reject(ErrorCode::allocation_failure,start); }
    catch(const std::length_error&) { return reject(ErrorCode::resource_limit,start); }
    for(std::size_t i=0;i<count;++i) {
        unsigned character=0;
        for(unsigned bit=0;bit<8;++bit) character=(character<<1)|get_bit(input_,position+payload_padding+i*8+bit);
        if(!character_valid(character,kind)) return reject(ErrorCode::constraint_violation,position+payload_padding+i*8);
        value[i]=static_cast<char>(character);
    }
    cursor_bit_=end; charge_wire(end-start);
    return Result<std::string>::success(std::move(value));
}

Result<BitString> BitReader::read_bit_string_owned(std::size_t lower,
    std::size_t upper, bool unconstrained, bool extensible) {
    auto live = validate_live();
    if(!live) return Result<BitString>::failure(live.error());
    const auto start = cursor_bit_;
    auto reject = [&](ErrorCode code,std::size_t offset) {
        return Result<BitString>::failure(fail({code,offset}).error());
    };
    if((unconstrained && (lower || upper || extensible)) || (!unconstrained && (lower > upper || upper > 65535)))
        return reject(ErrorCode::invalid_argument,start);
    const auto root_lower = lower, root_upper = upper;
    bool extension = false;
    if(extensible) {
        if(start >= logical_bit_limit_) return reject(ErrorCode::truncated_input,logical_bit_limit_);
        extension = get_bit(input_,start) != 0;
        if(extension) { unconstrained = true; lower = upper = 0; }
    }
    const auto content_start = start + static_cast<std::size_t>(extensible);
    const bool variable = unconstrained || lower != upper;
    const auto range = unconstrained ? std::size_t{0} : upper - lower + 1;
    const auto initial_padding = unconstrained || (!variable && upper > 16) ||
        (variable && range >= 256) ? (8 - content_start % 8) % 8 : 0;
    std::size_t position = 0;
    if(!checked_add_size(content_start,initial_padding,position)) return reject(ErrorCode::resource_limit,start);
    const auto width = variable ? (unconstrained ? 8u : octet_length_width(lower,upper)) : 0u;
    auto available = [&](std::size_t bits) {
        return position <= logical_bit_limit_ && bits <= logical_bit_limit_ - position;
    };
    if(!available(width)) return reject(ErrorCode::truncated_input,logical_bit_limit_);
    std::size_t encoded_length = 0;
    for(unsigned i=0;i<width;++i) encoded_length = (encoded_length << 1) | get_bit(input_,position+i);
    position += width;
    bool noncanonical = false;
    if(unconstrained) {
        if(encoded_length >= 192) return reject(ErrorCode::resource_limit,start);
        if(encoded_length >= 128) {
            if(!available(8)) return reject(ErrorCode::truncated_input,logical_bit_limit_);
            encoded_length = (encoded_length & 63u) << 8;
            for(unsigned i=0;i<8;++i) encoded_length |= static_cast<std::size_t>(get_bit(input_,position+i)) << (7-i);
            position += 8;
            noncanonical = encoded_length < 128;
        }
    } else if(variable && encoded_length >= range) return reject(ErrorCode::constraint_violation,start);
    const auto count = unconstrained ? encoded_length : lower + encoded_length;
    if(extension && count >= root_lower && count <= root_upper) return reject(ErrorCode::constraint_violation,start);
    const auto payload_padding = variable ? (8 - position % 8) % 8 : 0;
    std::size_t end = 0;
    if(!checked_add_size(position,payload_padding,end) || !checked_add_size(end,count,end))
        return reject(ErrorCode::resource_limit,start);
    auto ready = preflight(end-start,start);
    if(!ready) return Result<BitString>::failure(ready.error());
    for(std::size_t i=0;i<initial_padding;++i)
        if(get_bit(input_,content_start+i)) return reject(ErrorCode::nonzero_padding,content_start+i);
    for(std::size_t i=0;i<payload_padding;++i)
        if(get_bit(input_,position+i)) return reject(ErrorCode::nonzero_padding,position+i);
    if(noncanonical) return reject(ErrorCode::constraint_violation,start);
    BitString value;
    std::size_t octets = 0; (void)checked_bits_to_octets(count,octets);
    if(octets > value.octets.max_size()) return reject(ErrorCode::resource_limit,start);
    try { value.octets.resize(octets,std::byte{0}); }
    catch(const std::bad_alloc&) { return reject(ErrorCode::allocation_failure,start); }
    catch(const std::length_error&) { return reject(ErrorCode::resource_limit,start); }
    for(std::size_t i=0;i<count;++i) set_bit(value.octets,i,get_bit(input_,position+payload_padding+i)!=0);
    value.bit_count=count; cursor_bit_=end; charge_wire(end-start);
    return Result<BitString>::success(std::move(value));
}

Result<void> BitWriter::write_bit_string(const BitString& value,
    std::size_t lower,std::size_t upper,bool unconstrained, bool extensible) {
    auto live = validate_live();
    if(!live) return live;
    const auto start = cursor_bit_;
    if((unconstrained && (lower || upper || extensible)) || (!unconstrained && (lower > upper || upper > 65535)))
        return fail({ErrorCode::invalid_argument,start});
    const bool extension = extensible && (value.bit_count < lower || value.bit_count > upper);
    if(extension) { unconstrained = true; lower = upper = 0; }
    std::size_t content_start = 0;
    if(!checked_add_size(start, static_cast<std::size_t>(extensible), content_start)) return fail({ErrorCode::resource_limit,start});
    if(unconstrained && value.bit_count > 16383) return fail({ErrorCode::resource_limit,start});
    if(!unconstrained && (value.bit_count < lower || value.bit_count > upper)) return fail({ErrorCode::constraint_violation,start});
    std::size_t storage_octets=0; (void)checked_bits_to_octets(value.bit_count,storage_octets);
    if(value.octets.size()!=storage_octets || (value.bit_count%8 &&
       (std::to_integer<unsigned>(value.octets.back()) & ((1u << (8-value.bit_count%8))-1u))))
        return fail({ErrorCode::constraint_violation,start});
    const bool variable=unconstrained || lower!=upper;
    const auto range=unconstrained ? std::size_t{0} : upper-lower+1;
    const auto initial_padding=unconstrained || (!variable && upper>16) ||
        (variable && range>=256) ? (8-content_start%8)%8 : 0;
    const auto width=variable ? (unconstrained ? (value.bit_count<128?8u:16u) : octet_length_width(lower,upper)) : 0u;
    std::size_t position=0,end=0;
    if(!checked_add_size(content_start,initial_padding+width,position)) return fail({ErrorCode::resource_limit,start});
    const auto payload_padding=variable ? (8-position%8)%8 : 0;
    if(!checked_add_size(position,payload_padding,end) || !checked_add_size(end,value.bit_count,end))
        return fail({ErrorCode::resource_limit,start});
    auto ready=preflight(end-start,end,start);
    if(!ready) return ready;
    std::size_t octets=0; (void)checked_bits_to_octets(end,octets);
    auto grown=grow_to(octets,start);
    if(!grown) return grown;
    if(extensible) set_bit(output_,start,extension);
    for(std::size_t i=0;i<initial_padding;++i) set_bit(output_,content_start+i,false);
    const auto determinant=unconstrained ? value.bit_count | (width==16?0x8000u:0u) : value.bit_count-lower;
    for(unsigned i=0;i<width;++i) set_bit(output_,content_start+initial_padding+i,((determinant>>(width-i-1))&1u)!=0);
    for(std::size_t i=0;i<payload_padding;++i) set_bit(output_,position+i,false);
    for(std::size_t i=0;i<value.bit_count;++i) set_bit(output_,position+payload_padding+i,get_bit(value.octets,i)!=0);
    cursor_bit_=end; charge_wire(end-start); publish_output(octets);
    return Result<void>::success();
}

// Bounded wide BIT SIZE uses unconstrained length framing (units are bits).
// Scan/preflight/allocation precede publication of any state or output bit.
Result<BitString> BitReader::read_bit_string_owned_fragmented_size(
    std::size_t lower, std::size_t upper) {
    auto live = validate_live();
    if(!live) return Result<BitString>::failure(live.error());
    const auto start = cursor_bit_;
    auto reject = [&](ErrorCode code, std::size_t offset) {
        return Result<BitString>::failure(fail({code, offset}).error());
    };
    if(lower > upper || upper < 65536 || upper > 131072)
        return reject(ErrorCode::invalid_argument, start);
    const auto padding = (8 - start % 8) % 8;
    std::size_t position = 0;
    if(!checked_add_size(start, padding, position)) return reject(ErrorCode::resource_limit, start);
    const auto frame_start = position;
    std::size_t count = 0;
    bool noncanonical = false;
    auto take = [&](unsigned width, std::size_t& number) {
        if(position > logical_bit_limit_ || width > logical_bit_limit_ - position) return false;
        number = 0;
        for(unsigned i = 0; i < width; ++i) number = (number << 1) | get_bit(input_, position++);
        return true;
    };
    for(;;) {
        std::size_t first = 0, units = 0;
        if(!take(8, first)) return reject(ErrorCode::truncated_input, logical_bit_limit_);
        const bool fragment = first >= 192;
        if(first < 128) units = first;
        else if(first < 192) {
            std::size_t low = 0;
            if(!take(8, low)) return reject(ErrorCode::truncated_input, logical_bit_limit_);
            units = ((first & 63u) << 8) | low;
            noncanonical = noncanonical || units < 128;
        } else {
            if(first < 193 || first > 196) return reject(ErrorCode::constraint_violation, start);
            units = (first & 63u) * 16384;
        }
        if(!checked_add_size(count, units, count) || count > 131072)
            return reject(ErrorCode::resource_limit, start);
        if(position > logical_bit_limit_ || units > logical_bit_limit_ - position)
            return reject(ErrorCode::truncated_input, logical_bit_limit_);
        position += units;
        if(!fragment) break;
    }
    const auto end = position;
    auto ready = preflight(end - start, start);
    if(!ready) return Result<BitString>::failure(ready.error());
    for(std::size_t i = 0; i < padding; ++i)
        if(get_bit(input_, start + i)) return reject(ErrorCode::nonzero_padding, start + i);
    // Require maximal 16K multiples, matching the canonical encoder.
    position = frame_start;
    std::size_t remaining_count = count;
    for(;;) {
        std::size_t first = 0, units = 0;
        (void)take(8, first);
        const bool fragment = first >= 192;
        if(first < 128) units = first;
        else if(first < 192) { std::size_t low = 0; (void)take(8, low); units = ((first & 63u) << 8) | low; }
        else units = (first & 63u) * 16384;
        const auto expected = remaining_count >= 16384 ? std::min(std::size_t{4}, remaining_count / 16384) * 16384 : remaining_count;
        if(units != expected || fragment != (remaining_count >= 16384)) noncanonical = true;
        remaining_count -= units; position += units;
        if(!fragment) break;
    }
    if(noncanonical || count < lower || count > upper) return reject(ErrorCode::constraint_violation, start);
    BitString value;
    std::size_t octets = 0;
    (void)checked_bits_to_octets(count, octets);
    try { value.octets.resize(octets, std::byte{0}); }
    catch(const std::bad_alloc&) { return reject(ErrorCode::allocation_failure, start); }
    catch(const std::length_error&) { return reject(ErrorCode::resource_limit, start); }
    position = frame_start;
    std::size_t copied = 0;
    for(;;) {
        std::size_t first = 0, units = 0;
        (void)take(8, first);
        const bool fragment = first >= 192;
        if(first < 128) units = first;
        else if(first < 192) { std::size_t low = 0; (void)take(8, low); units = ((first & 63u) << 8) | low; }
        else units = (first & 63u) * 16384;
        for(std::size_t i = 0; i < units; ++i) set_bit(value.octets, copied + i, get_bit(input_, position + i) != 0);
        copied += units; position += units;
        if(!fragment) break;
    }
    value.bit_count = count;
    cursor_bit_ = end; charge_wire(end - start);
    return Result<BitString>::success(std::move(value));
}

Result<void> BitWriter::write_bit_string_fragmented_size(const BitString& value,
    std::size_t lower, std::size_t upper) {
    auto live = validate_live();
    if(!live) return live;
    const auto start = cursor_bit_;
    if(lower > upper || upper < 65536 || upper > 131072) return fail({ErrorCode::invalid_argument, start});
    if(value.bit_count < lower || value.bit_count > upper) return fail({ErrorCode::constraint_violation, start});
    std::size_t storage = 0;
    (void)checked_bits_to_octets(value.bit_count, storage);
    if(value.octets.size() != storage || (value.bit_count % 8 &&
       (std::to_integer<unsigned>(value.octets.back()) & ((1u << (8 - value.bit_count % 8)) - 1u))))
        return fail({ErrorCode::constraint_violation, start});
    const auto padding = (8 - start % 8) % 8;
    auto remaining = value.bit_count;
    std::size_t determinant_bits = 0;
    while(remaining >= 16384) {
        const auto chunk = std::min(std::size_t{4}, remaining / 16384) * 16384;
        remaining -= chunk; determinant_bits += 8;
    }
    determinant_bits += remaining < 128 ? 8 : 16;
    std::size_t total = 0, end = 0;
    if(!checked_add_size(padding, determinant_bits, total) || !checked_add_size(total, value.bit_count, total) ||
       !checked_add_size(start, total, end)) return fail({ErrorCode::resource_limit, start});
    auto ready = preflight(total, end, start);
    if(!ready) return ready;
    std::size_t octets = 0; (void)checked_bits_to_octets(end, octets);
    auto grown = grow_to(octets, start);
    if(!grown) return grown;
    std::size_t position = start;
    for(std::size_t i = 0; i < padding; ++i) set_bit(output_, position++, false);
    auto put = [&](std::size_t number, unsigned width) {
        for(unsigned i = 0; i < width; ++i) set_bit(output_, position++, ((number >> (width - i - 1)) & 1u) != 0);
    };
    remaining = value.bit_count;
    std::size_t copied = 0;
    for(;;) {
        const bool fragment = remaining >= 16384;
        const auto chunk = fragment ? std::min(std::size_t{4}, remaining / 16384) * 16384 : remaining;
        if(fragment) put(192 + chunk / 16384, 8);
        else put(chunk | (chunk >= 128 ? 0x8000u : 0u), chunk < 128 ? 8u : 16u);
        for(std::size_t i = 0; i < chunk; ++i) set_bit(output_, position++, get_bit(value.octets, copied + i) != 0);
        copied += chunk; remaining -= chunk;
        if(!fragment) break;
    }
    cursor_bit_ = end; charge_wire(total); publish_output(octets);
    return Result<void>::success();
}

namespace {
template<class Function> struct ScopeExit {
    Function function;
    ~ScopeExit() noexcept { function(); }
};
template<class Function> ScopeExit<Function> on_exit(Function function) {
    return {std::move(function)};
}

// Map a logical content bit across determinant gaps without fragment metadata.
// A non-final boundary belongs to the following content; EOF precedes a zero
// terminal determinant. The containing frame has already passed all scans.
bool payload_bit_position(std::span<const std::byte> input, std::size_t limit,
                          std::size_t frame_start, std::size_t logical,
                          std::size_t payload_bits, std::size_t& position) noexcept {
    auto cursor = frame_start;
    std::size_t consumed = 0;
    auto take = [&](unsigned width, std::size_t& number) {
        if(cursor > limit || width > limit - cursor) return false;
        number = 0;
        for(unsigned i = 0; i < width; ++i) number = (number << 1) | get_bit(input, cursor++);
        return true;
    };
    for(;;) {
        const auto padding = (8 - cursor % 8) % 8;
        if(cursor > limit || padding > limit - cursor) return false;
        cursor += padding;
        std::size_t first = 0, units = 0;
        if(!take(8, first)) return false;
        bool fragment = false;
        if(first < 128) units = first;
        else if(first < 192) {
            std::size_t low = 0;
            if(!take(8, low)) return false;
            units = ((first & 63u) << 8) | low;
        } else {
            if(first < 193 || first > 196) return false;
            units = (first & 63u) * 16384;
            fragment = true;
        }
        std::size_t bits = 0, next = 0;
        if(!checked_octets_to_bits(units, bits) || !checked_add_size(consumed, bits, next) ||
           cursor > limit || bits > limit - cursor) return false;
        if(logical < next || (logical == next && logical == payload_bits)) {
            position = cursor + logical - consumed;
            return true;
        }
        cursor += bits;
        consumed = next;
        if(!fragment) return false;
    }
}

bool framed_octets(std::size_t payload, std::size_t& total) noexcept {
    total = payload;
    auto remaining = payload;
    while(remaining >= 16384) {
        const auto blocks = std::min<std::size_t>(4, remaining / 16384);
        if(!checked_add_size(total, 1, total)) return false;
        remaining -= blocks * 16384;
    }
    return checked_add_size(total, remaining < 128 ? 1 : 2, total);
}
} // namespace

Error BitReader::map_known_error(Error error) const noexcept {
    if(!known_child_) return error;
    std::size_t enclosing = 0;
    if(error.bit_offset > logical_bit_limit_ ||
       !payload_bit_position(known_origin_->input_, known_origin_->logical_bit_limit_,
                             known_frame_start_, error.bit_offset, logical_bit_limit_, enclosing)) {
        error.code = ErrorCode::invalid_argument;
        enclosing = known_frame_start_;
    }
    error.bit_offset = enclosing;
    return known_origin_->map_known_error(error);
}

Result<void> BitReader::read_known_open_type_impl(
    void* argument, Result<void> (*callback)(FieldReader&, void*)) {
    auto live = validate_live();
    if(!live) return live;
    const auto start = cursor_bit_;
    auto scanned = scan_extension_frame(input_, logical_bit_limit_, start, false,
                                        FramingPass::availability, 0);
    if(!scanned) return fail(scanned.error());
    const auto frame = scanned.value();
    auto ready = preflight(frame.end - start, start);
    if(!ready) return ready;
    std::size_t staging = 0;
    std::vector<std::byte> payload;
    if(context_->known_open_depth_ >= context_->limits_.max_known_open_depth ||
       !checked_add_size(context_->known_open_staging_octets_, frame.units, staging) ||
       staging > context_->limits_.max_known_open_staging_octets || frame.units > payload.max_size())
        return fail({ErrorCode::resource_limit, start});
    for(auto pass : {FramingPass::padding, FramingPass::canonical}) {
        auto checked = scan_extension_frame(input_, logical_bit_limit_, start, false, pass, frame.units);
        if(!checked) return fail(checked.error());
    }
    auto* context = context_;
    const auto wire = context->wire_bits_, elements = context->collection_elements_;
    const auto bitmap = context->extension_bitmap_bits_, retained = context->retained_unknown_payload_octets_;
    const auto records = context->retained_unknown_records_, depth = context->known_open_depth_;
    const auto previous_view = context->known_active_reader_;
    bool committed = false;
    context->known_open_staging_octets_ = staging;
    ++context->known_open_depth_;
    auto cleanup = on_exit([&]() noexcept {
        context->known_open_staging_octets_ -= frame.units;
        context->known_open_depth_ = depth;
        context->known_active_reader_ = previous_view;
        if(!committed) {
            context->wire_bits_ = wire;
            context->collection_elements_ = elements;
            context->extension_bitmap_bits_ = bitmap;
            context->retained_unknown_payload_octets_ = retained;
            context->retained_unknown_records_ = records;
        }
    });
    try {
        payload.resize(frame.units);
        auto copied = scan_extension_frame(input_, logical_bit_limit_, start, false,
                                            FramingPass::copy, frame.units, &payload);
        if(!copied) return fail(copied.error());
        std::size_t bits = 0;
        (void)checked_octets_to_bits(frame.units, bits);
        BitReader child(payload, *context, bits);
        child.known_child_ = true;
        child.known_origin_ = this;
        child.known_frame_start_ = start;
        context->known_active_reader_ = &child;
        FieldReader fields(child);
        try {
            auto decoded = callback(fields, argument);
            if(context->failed_) return Result<void>::failure(context->error_);
            if(!decoded) return child.fail(decoded.error());
            auto complete = child.validate_complete_value();
            if(!complete) return complete;
        } catch(const std::bad_alloc&) {
            return child.fail({ErrorCode::allocation_failure, child.cursor_bit_});
        } catch(const std::length_error&) {
            return child.fail({ErrorCode::resource_limit, child.cursor_bit_});
        } catch(...) {
            // The anchor is in this stream; this reader maps it only if this
            // known field is itself inside another decoded known payload.
            if(!context->failed_) (void)fail({ErrorCode::invalid_state, start});
            throw;
        }
        cursor_bit_ = frame.end;
        charge_wire(frame.end - start);
        committed = true;
        return Result<void>::success();
    } catch(const std::bad_alloc&) {
        return fail({ErrorCode::allocation_failure, start});
    } catch(const std::length_error&) {
        return fail({ErrorCode::resource_limit, start});
    }
}

Result<void> BitWriter::write_known_open_type_impl(
    void* argument, Result<void> (*callback)(FieldWriter&, void*)) {
    auto live = validate_live();
    if(!live) return live;
    const auto start = cursor_bit_;
    if(context_->known_open_depth_ >= context_->limits_.max_known_open_depth)
        return fail({ErrorCode::resource_limit, start});
    auto* context = context_;
    const auto wire = context->wire_bits_, elements = context->collection_elements_;
    const auto logical_output = context->logical_output_octets_, depth = context->known_open_depth_;
    const auto previous_view = context->known_active_writer_;
    BitWriter child(*context);
    child.known_child_ = true;
    child.known_encode_anchor_ = known_child_ ? known_encode_anchor_ : start;
    bool committed = false;
    ++context->known_open_depth_;
    context->known_active_writer_ = &child;
    auto cleanup = on_exit([&]() noexcept {
        context->known_open_staging_octets_ -= child.known_staged_octets_;
        context->known_open_depth_ = depth;
        context->known_active_writer_ = previous_view;
        if(!committed) {
            context->wire_bits_ = wire;
            context->collection_elements_ = elements;
            context->logical_output_octets_ = logical_output;
        }
    });
    try {
        FieldWriter fields(child);
        auto encoded = callback(fields, argument);
        if(context->failed_) return Result<void>::failure(context->error_);
        if(!encoded) return child.fail(encoded.error());
        auto complete = child.finish();
        if(!complete) return Result<void>::failure(complete.error());
        const auto& payload = complete.value().octets;
        std::size_t octets = 0, frame_bits = 0, total = 0, end = 0;
        const auto padding = (8 - start % 8) % 8;
        if(!framed_octets(payload.size(), octets) || !checked_octets_to_bits(octets, frame_bits) ||
           !checked_add_size(padding, frame_bits, total) || !checked_add_size(start, total, end))
            return fail({ErrorCode::resource_limit, start});
        // Admit the enclosing view for its final atomic append. The callback
        // has returned and the child is locally complete.
        context->known_active_writer_ = previous_view;
        auto ready = preflight(total, end, start);
        if(!ready) return ready;
        std::size_t output_octets = 0;
        (void)checked_bits_to_octets(end, output_octets);
        auto grown = grow_to(output_octets, start);
        if(!grown) return grown;
        auto position = start;
        auto emit = [&](std::uint64_t value, unsigned width) {
            for(unsigned i = width; i > 0; --i)
                set_bit(output_, position++, ((value >> (i - 1)) & 1u) != 0);
        };
        emit(0, static_cast<unsigned>(padding));
        std::size_t consumed = 0;
        while(payload.size() - consumed >= 16384) {
            const auto blocks = std::min<std::size_t>(4, (payload.size() - consumed) / 16384);
            emit(0xc0 + blocks, 8);
            for(std::size_t i = 0; i < blocks * 16384; ++i)
                emit(std::to_integer<unsigned>(payload[consumed++]), 8);
        }
        const auto remaining = payload.size() - consumed;
        if(remaining < 128) emit(remaining, 8);
        else { emit(0x80u | (remaining >> 8), 8); emit(remaining & 255, 8); }
        for(; consumed < payload.size(); ++consumed)
            emit(std::to_integer<unsigned>(payload[consumed]), 8);
        cursor_bit_ = end;
        charge_wire(total);
        publish_output(output_octets);
        committed = true;
        return Result<void>::success();
    } catch(const std::bad_alloc&) {
        return child.fail({ErrorCode::allocation_failure, child.cursor_bit_});
    } catch(const std::length_error&) {
        return child.fail({ErrorCode::resource_limit, child.cursor_bit_});
    } catch(...) {
        if(!context->failed_) (void)fail({ErrorCode::invalid_state, start});
        throw;
    }
}

} // namespace nrforge::aper

namespace nrforge::aper {
bool object_identifier_content_valid(std::span<const std::byte> value) noexcept {
    if(value.empty()) return false;
    bool first = true;
    for(auto byte : value) {
        const auto octet = std::to_integer<unsigned>(byte);
        if(first && octet == 128) return false;
        first = !(octet & 128u);
    }
    return first;
}
Result<void> write_object_identifier(FieldWriter& f, std::span<const std::byte> value) {
    if(!object_identifier_content_valid(value))
        return f.record_failure({ErrorCode::constraint_violation, f.cursor_bit()});
    return f.write_octet_string(value, 0, 0, true);
}
Result<std::vector<std::byte>> BitReader::read_object_identifier_owned() {
    auto live = validate_live();
    if(!live) return Result<std::vector<std::byte>>::failure(live.error());
    const auto start = cursor_bit_;
    const auto wire = context_->wire_bits_;
    auto value = read_octet_string_owned(0, 0, true);
    if(!value) return value;
    if(!object_identifier_content_valid(value.value())) {
        cursor_bit_ = start;
        context_->wire_bits_ = wire;
        return Result<std::vector<std::byte>>::failure(fail({ErrorCode::constraint_violation, start}).error());
    }
    return value;
}
Result<std::vector<std::byte>> read_object_identifier(FieldReader& f) {
    return f.read_object_identifier_owned();
}
}

namespace nrforge::aper {
Result<void> write_private_open_payload(FieldWriter& f, std::span<const std::byte> value) {
    if(value.empty()) return f.record_failure({ErrorCode::constraint_violation, f.cursor_bit()});
    return f.write_octet_string(value, 0, 0, true);
}
Result<std::vector<std::byte>> read_private_open_payload(FieldReader& f) {
    return f.read_open_type_owned();
}
}
