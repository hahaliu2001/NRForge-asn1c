#include <runtime.hpp>
#include <sequence_extensions.hpp>
#include "main/types.hpp"
#include "main/mapping.hpp"
#include "main/codec.hpp"
#include "compat/types.hpp"
#include "compat/mapping.hpp"
#include "compat/codec.hpp"
#include "reversed/types.hpp"
#include "reversed/mapping.hpp"
#include "reversed/codec.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <type_traits>

#ifndef NDEBUG
#error "Generated checks must run under NDEBUG."
#endif
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); std::abort(); } } while(0)
namespace c = extensiontest;
using namespace nrforge::aper;
namespace {
long allocation_countdown = -1;
static_assert(c::Inner_aper::extensible && c::Inner_aper::root_field_count == 2);
static_assert(c::Inner_aper::known_addition_count == 0);
static_assert(c::Empty_aper::root_field_count == 0);
static_assert(std::is_same_v<c::Inner_aper::extension_data_type, SequenceExtensionData>);
static_assert(c::Collision_aper::extension_data_member == &c::Collision::sequence_extensions_2);
static_assert(sizeof(UnknownSequenceAddition::addition_index) == sizeof(std::uint64_t));
static_assert(std::is_same_v<decltype(SequenceExtensionData::received_bitmap_bit_count), std::size_t>);

template<class R> void error(const R& value, ErrorCode code, std::size_t offset) {
    REQUIRE(!value);
    if(value.error().code != code || value.error().bit_offset != offset)
        std::fprintf(stderr, "expected(%d,%zu), got(%d,%zu)\n", static_cast<int>(code), offset,
                     static_cast<int>(value.error().code), value.error().bit_offset);
    REQUIRE(value.error().code == code && value.error().bit_offset == offset);
}
bool equal(const SequenceExtensionData& a, const SequenceExtensionData& b) {
    if(a.received_bitmap_bit_count != b.received_bitmap_bit_count ||
       a.unknown_additions.size() != b.unknown_additions.size()) return false;
    for(std::size_t i = 0; i < a.unknown_additions.size(); ++i)
        if(a.unknown_additions[i].addition_index != b.unknown_additions[i].addition_index ||
           a.unknown_additions[i].payload_octets != b.unknown_additions[i].payload_octets) return false;
    return true;
}
bool equal(const c::Inner& a, const c::Inner& b) {
    return a.value == b.value && a.flag == b.flag &&
        equal(a.*c::Inner_aper::extension_data_member, b.*c::Inner_aper::extension_data_member);
}
bool equal(const c::Outer& a, const c::Outer& b) {
    return equal(a.inner, b.inner) && a.flag == b.flag &&
        equal(a.*c::Outer_aper::extension_data_member, b.*c::Outer_aper::extension_data_member);
}
bool equal(const c::Empty& a, const c::Empty& b) {
    return equal(a.*c::Empty_aper::extension_data_member, b.*c::Empty_aper::extension_data_member);
}
bool equal(const c::Collision& a, const c::Collision& b) {
    return a.sequence_extensions == b.sequence_extensions &&
        a.sequence_extensions_1 == b.sequence_extensions_1 &&
        equal(a.*c::Collision_aper::extension_data_member, b.*c::Collision_aper::extension_data_member);
}

// Independent BASIC APER wire model. It emits roots and literal length headers,
// never invokes a generated codec/runtime writer to derive an expected byte.
struct Bits {
    std::vector<bool> bits;
    std::vector<std::size_t> padding;
    void put(std::uint64_t value, unsigned count) {
        for(unsigned i = count; i > 0; --i) bits.push_back(((value >> (i - 1)) & 1u) != 0);
    }
    void align() {
        while(bits.size() % 8) { padding.push_back(bits.size()); bits.push_back(false); }
    }
    void integer(std::uint64_t value) {
        unsigned octets = 1;
        for(auto rest = value; rest > 255; rest >>= 8) ++octets;
        put(octets - 1, 2);
        align();
        put(value, octets * 8);
    }
    void length(std::size_t count) {
        REQUIRE(count < 16384);
        if(count < 128) put(count, 8);
        else { put(0x80u | (count >> 8), 8); put(count & 255, 8); }
    }
    void open(const std::vector<std::byte>& payload) {
        align();
        std::size_t offset = 0;
        while(payload.size() - offset >= 16384) {
            const auto blocks = std::min<std::size_t>(4, (payload.size() - offset) / 16384);
            put(0xc0 + blocks, 8);
            for(std::size_t i = 0; i < blocks * 16384; ++i)
                put(std::to_integer<unsigned>(payload[offset + i]), 8);
            offset += blocks * 16384;
        }
        length(payload.size() - offset);
        for(; offset < payload.size(); ++offset) put(std::to_integer<unsigned>(payload[offset]), 8);
    }
    void extensions(const SequenceExtensionData& data) {
        const auto width = data.received_bitmap_bit_count;
        REQUIRE(width > 0 && width < 16384);
        if(width <= 64) { put(0, 1); put(width - 1, 6); }
        else { put(1, 1); align(); length(width); }
        for(std::size_t i = 0; i < width; ++i) {
            const bool present = std::any_of(data.unknown_additions.begin(), data.unknown_additions.end(),
                [i](const auto& record) { return record.addition_index == i; });
            put(present, 1);
        }
        for(const auto& record : data.unknown_additions) open(record.payload_octets);
    }
    std::vector<std::byte> finish() {
        if(bits.empty()) bits.resize(8, false);
        align();
        std::vector<std::byte> output(bits.size() / 8, std::byte{0});
        for(std::size_t i = 0; i < bits.size(); ++i)
            if(bits[i]) output[i / 8] |= static_cast<std::byte>(0x80u >> (i % 8));
        return output;
    }
};
void model(Bits& bits, const c::Inner& value) {
    const auto& data = value.*c::Inner_aper::extension_data_member;
    bits.put(data.received_bitmap_bit_count != 0, 1);
    bits.put(value.flag.has_value(), 1);
    bits.integer(value.value);
    if(value.flag) bits.put(*value.flag, 1);
    if(data.received_bitmap_bit_count) bits.extensions(data);
}
void model(Bits& bits, const c::Outer& value) {
    const auto& data = value.*c::Outer_aper::extension_data_member;
    bits.put(data.received_bitmap_bit_count != 0, 1);
    bits.put(value.flag.has_value(), 1);
    model(bits, value.inner);
    if(value.flag) bits.put(*value.flag, 1);
    if(data.received_bitmap_bit_count) bits.extensions(data);
}
void model(Bits& bits, const c::Empty& value) {
    const auto& data = value.*c::Empty_aper::extension_data_member;
    bits.put(data.received_bitmap_bit_count != 0, 1);
    if(data.received_bitmap_bit_count) bits.extensions(data);
}
void model(Bits& bits, const c::Collision& value) {
    const auto& data = value.*c::Collision_aper::extension_data_member;
    bits.put(data.received_bitmap_bit_count != 0, 1);
    bits.put(value.sequence_extensions_1.has_value(), 1);
    bits.put(value.sequence_extensions, 1);
    if(value.sequence_extensions_1) bits.put(*value.sequence_extensions_1, 1);
    if(data.received_bitmap_bit_count) bits.extensions(data);
}
#define ADAPTER(T,BASE) struct T##Adapter { using Type=c::T; static auto put(FieldWriter& f,const Type&v){return c::compound_codec::put_##T(f,v);} static auto get(FieldReader&f){return c::compound_codec::get_##T(f);} static auto encode(const Type&v,const Limits&l={}){return c::encode_##BASE(v,l);} static auto decode(std::span<const std::byte>w,const Limits&l={}){return c::decode_##BASE(w,l);} };
ADAPTER(Inner,inner)
ADAPTER(Outer,outer)
ADAPTER(Empty,empty)
ADAPTER(Collision,collision)
#undef ADAPTER

template<class Adapter> void root_only(const typename Adapter::Type& value) {
    for(unsigned residue = 0; residue < 8; ++residue) {
        Bits bits;
        for(unsigned i = 0; i < residue; ++i) bits.put(1, 1);
        model(bits, value);
        const auto end = bits.bits.size();
        auto wire = bits.finish();
        auto encoded = encode_complete(value, Limits{}, [&](FieldWriter& fields) {
            for(unsigned i = 0; i < residue; ++i) REQUIRE(fields.write_bit(true));
            return Adapter::put(fields, value);
        });
        REQUIRE(encoded && encoded.value().octets == wire);
        REQUIRE(encoded.value().last_field_end_bit == end);
        auto decoded = decode_complete<typename Adapter::Type>(wire, Limits{}, [&](FieldReader& fields) {
            for(unsigned i = 0; i < residue; ++i) REQUIRE(fields.read_bit().value());
            return Adapter::get(fields);
        });
        REQUIRE(decoded && equal(decoded.value(), value));
        if(residue == 0) {
            REQUIRE(Adapter::encode(value).value().octets == wire);
            REQUIRE(equal(Adapter::decode(wire).value(), value));
            for(std::size_t n = 0; n < wire.size(); ++n)
                error(Adapter::decode(std::span<const std::byte>(wire.data(), n)), ErrorCode::truncated_input, n * 8);
            auto trailing = wire;
            trailing.push_back(std::byte{0});
            error(Adapter::decode(trailing), ErrorCode::trailing_data, wire.size() * 8);
            for(auto position : bits.padding) {
                auto bad = wire;
                bad[position / 8] |= static_cast<std::byte>(0x80u >> (position % 8));
                error(Adapter::decode(bad), ErrorCode::nonzero_padding, position);
            }
        }
    }
}
std::vector<std::byte> patterned(std::size_t count) {
    std::vector<std::byte> value(count);
    for(std::size_t i = 0; i < count; ++i) value[i] = static_cast<std::byte>((i * 19 + i / 257) % 256);
    return value;
}
SequenceExtensionData sidecar(std::size_t width, std::uint64_t index, std::size_t length) {
    SequenceExtensionData result;
    result.received_bitmap_bit_count = width;
    result.unknown_additions.push_back({index, patterned(length)});
    return result;
}
void unknown_and_ownership() {
    for(auto width : {std::size_t{3}, std::size_t{65}, std::size_t{128}}) {
        for(auto length : {std::size_t{1}, std::size_t{128}, std::size_t{16384}, std::size_t{65536}}) {
            c::Inner expected;
            expected.value = 65536;
            expected.flag = false;
            expected.*c::Inner_aper::extension_data_member = sidecar(width, width - 2, length);
            for(unsigned residue = 0; residue < 8; ++residue) {
                Bits bits;
                for(unsigned i = 0; i < residue; ++i) bits.put(1, 1);
                model(bits, expected);
                auto wire = bits.finish();
                auto decoded = decode_complete<c::Inner>(wire, Limits{}, [&](FieldReader& fields) {
                    for(unsigned i = 0; i < residue; ++i) REQUIRE(fields.read_bit().value());
                    return c::compound_codec::get_Inner(fields);
                });
                REQUIRE(decoded && equal(decoded.value(), expected));
                wire.clear();
                wire.shrink_to_fit();
                REQUIRE(equal(decoded.value(), expected));
                auto copy = decoded.value();
                (copy.*c::Inner_aper::extension_data_member).unknown_additions[0].payload_octets[0] ^= std::byte{0xff};
                REQUIRE(!equal(copy, decoded.value()));
                auto moved = std::move(decoded).value();
                REQUIRE(equal(moved, expected));
                EncodeContext context;
                BitWriter writer(context);
                for(unsigned i = 0; i < residue; ++i) REQUIRE(writer.write_bit(true));
                FieldWriter fields(writer);
                error(c::compound_codec::put_Inner(fields, moved), ErrorCode::constraint_violation, residue);
                REQUIRE(writer.cursor_bit() == residue && context.wire_bits() == residue);
                error(writer.finish(), ErrorCode::constraint_violation, residue);
            }
        }
    }
    c::Inner inner;
    inner.sequence_extensions.received_bitmap_bit_count = 1; // Width-only is retained data too.
    error(c::encode_inner(inner), ErrorCode::constraint_violation, 0);
    inner.sequence_extensions = {};
    inner.sequence_extensions.unknown_additions.push_back({0, {std::byte{0}}});
    error(c::encode_inner(inner), ErrorCode::constraint_violation, 0);
    c::Outer parent;
    parent.inner = inner;
    error(c::encode_outer(parent), ErrorCode::constraint_violation, 2);
    c::Envelope envelope;
    envelope.pick = c::Pick_inner{inner};
    error(c::encode_envelope(envelope), ErrorCode::constraint_violation, 2);
}
void nested_budgets_and_oom() {
    c::Outer expected;
    expected.inner.value = 255;
    expected.inner.sequence_extensions = sidecar(3, 0, 2);
    expected.inner.sequence_extensions.unknown_additions.push_back({2, patterned(1)});
    expected.flag = true;
    expected.sequence_extensions = sidecar(3, 2, 3);
    Bits bits;
    model(bits, expected);
    auto wire = bits.finish();
    for(unsigned budget = 0; budget < 3; ++budget) {
        for(bool less : {false, true}) {
            Limits limits;
            limits.max_extension_bitmap_bits = 6 - static_cast<std::size_t>(less && budget == 0);
            limits.max_retained_unknown_payload_octets = 6 - static_cast<std::size_t>(less && budget == 1);
            limits.max_retained_unknown_records = 3 - static_cast<std::size_t>(less && budget == 2);
            DecodeContext context(limits);
            auto made = BitReader::make(wire, context);
            REQUIRE(made);
            auto reader = std::move(made).value();
            FieldReader fields(reader);
            auto result = c::compound_codec::get_Outer(fields);
            if(less) {
                REQUIRE(!result && result.error().code == ErrorCode::resource_limit);
                REQUIRE(context.retained_unknown_records() == 2 && context.retained_unknown_payload_octets() == 3);
                REQUIRE(context.extension_bitmap_bits() == (budget == 0 ? 3 : 6));
                const auto at = reader.cursor_bit();
                error(fields.read_bit(), ErrorCode::resource_limit, result.error().bit_offset);
                REQUIRE(reader.cursor_bit() == at);
            } else {
                REQUIRE(result && equal(result.value(), expected));
                REQUIRE(context.extension_bitmap_bits() == 6 && context.retained_unknown_payload_octets() == 6 &&
                        context.retained_unknown_records() == 3);
            }
        }
    }
    bool bitmap_failure = false, payload_failure = false, container_failure = false;
    long point;
    for(point = 0; point < 100; ++point) {
        DecodeContext context;
        auto made = BitReader::make(wire, context);
        REQUIRE(made);
        auto reader = std::move(made).value();
        FieldReader fields(reader);
        allocation_countdown = point;
        auto decoded = c::compound_codec::get_Outer(fields);
        allocation_countdown = -1;
        if(decoded) { REQUIRE(equal(decoded.value(), expected)); break; }
        REQUIRE(decoded.error().code == ErrorCode::allocation_failure && context.failed());
        REQUIRE(decoded.error().bit_offset == reader.cursor_bit());
        const auto bitmap = context.extension_bitmap_bits();
        const auto records = context.retained_unknown_records();
        bitmap_failure = bitmap_failure || bitmap == 0;
        payload_failure = payload_failure || (bitmap == 3 && records == 0);
        container_failure = container_failure || records > 0;
        error(fields.read_open_type_owned(), ErrorCode::allocation_failure, decoded.error().bit_offset);
    }
    REQUIRE(point > 0 && point < 100 && bitmap_failure && payload_failure && container_failure);
    for(point = 0; point < 100; ++point) {
        allocation_countdown = point;
        auto decoded = c::decode_outer(wire);
        allocation_countdown = -1;
        if(decoded) { REQUIRE(equal(decoded.value(), expected)); break; }
        REQUIRE(decoded.error().code == ErrorCode::allocation_failure);
    }
    REQUIRE(point > 0 && point < 100);
    // Generated non-extensible parents move owning nested fields/wrappers.
    c::Envelope envelope;
    envelope.pick = c::Pick_inner{expected.inner};
    envelope.outer = expected;
    Bits parent;
    parent.put(1, 1); // outer present
    parent.put(1, 1); // CHOICE inner
    model(parent, expected.inner);
    model(parent, expected);
    auto parent_wire = parent.finish();
    auto decoded_parent = c::decode_envelope(parent_wire);
    REQUIRE(decoded_parent);
    REQUIRE(equal(std::get<c::Pick_inner>(decoded_parent.value().pick).value, expected.inner));
    REQUIRE(decoded_parent.value().outer && equal(*decoded_parent.value().outer, expected));
}
void malformed_generated_decode() {
    c::Inner expected;
    expected.value = 256;
    expected.sequence_extensions = sidecar(3, 0, 2);
    expected.sequence_extensions.unknown_additions.push_back({2, patterned(1)});
    Bits model_bits;
    model(model_bits, expected);
    auto wire = model_bits.finish();
    wire.pop_back(); // Second owned payload is truncated after an earlier record committed.
    DecodeContext context;
    auto made = BitReader::make(wire, context);
    REQUIRE(made);
    auto reader = std::move(made).value();
    FieldReader fields(reader);
    auto result = c::compound_codec::get_Inner(fields);
    error(result, ErrorCode::truncated_input, wire.size() * 8);
    REQUIRE(context.extension_bitmap_bits() == 3);
    REQUIRE(context.retained_unknown_payload_octets() == 2 && context.retained_unknown_records() == 1);
    const auto cursor = reader.cursor_bit();
    error(fields.record_failure({ErrorCode::allocation_failure, cursor}), ErrorCode::truncated_input, wire.size() * 8);
    REQUIRE(reader.cursor_bit() == cursor);
    error(c::decode_inner(wire), ErrorCode::truncated_input, wire.size() * 8);
}

void failure_hook_and_valueless() {
    const std::array<std::byte, 1> input{std::byte{0}};
    DecodeContext context;
    auto made = BitReader::make(input, context);
    REQUIRE(made);
    auto reader = std::move(made).value();
    FieldReader fields(reader);
    REQUIRE(fields.read_bit());
    error(fields.record_failure({ErrorCode::allocation_failure, 1}), ErrorCode::allocation_failure, 1);
    error(fields.record_failure({ErrorCode::resource_limit, 99}), ErrorCode::allocation_failure, 1);
    error(fields.read_bit(), ErrorCode::allocation_failure, 1);
    REQUIRE(reader.cursor_bit() == 1 && context.wire_bits() == 1);
    auto ignored = decode_complete<int>(input, Limits{}, [](FieldReader& f) {
        (void)f.record_failure({ErrorCode::allocation_failure, 0});
        return Result<int>::success(1);
    });
    error(ignored, ErrorCode::allocation_failure, 0);
    auto thrown = decode_complete<int>(input, Limits{}, [](FieldReader& f) -> Result<int> {
        (void)f.record_failure({ErrorCode::resource_limit, 0});
        throw std::bad_alloc();
    });
    error(thrown, ErrorCode::resource_limit, 0);
    DecodeContext lifecycle;
    auto owner = BitReader::make(input, lifecycle);
    REQUIRE(owner);
    auto original = std::move(owner).value();
    FieldReader old(original);
    auto transferred = std::move(original);
    error(old.record_failure({ErrorCode::allocation_failure, 0}), ErrorCode::invalid_state, 0);
    REQUIRE(!lifecycle.failed());
    REQUIRE(transferred.validate_complete_value());
    FieldReader finished(transferred);
    error(finished.record_failure({ErrorCode::allocation_failure, 0}), ErrorCode::invalid_state, 8);
    REQUIRE(!lifecycle.failed());

    c::Inner source;
    source.sequence_extensions = sidecar(3, 1, 2);
    c::Pick_inner wrapper{source};
    c::Pick choice{c::Pick_flag{true}};
    allocation_countdown = 0;
    try { choice.emplace<c::Pick_inner>(wrapper); REQUIRE(false); }
    catch(const std::bad_alloc&) {}
    allocation_countdown = -1;
    // This explicit construction is supported by our standard-library target;
    // vectors made this state reachable without undefined behavior.
    REQUIRE(choice.valueless_by_exception());
    error(c::encode_pick(choice), ErrorCode::constraint_violation, 0);
}
} // namespace

void* operator new(std::size_t size) {
    if(allocation_countdown >= 0) {
        if(allocation_countdown == 0) { allocation_countdown = -1; throw std::bad_alloc(); }
        --allocation_countdown;
    }
    if(void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

int main() {
    root_only<InnerAdapter>({});
    root_only<OuterAdapter>({});
    root_only<EmptyAdapter>({});
    for(auto value : {UINT64_C(0), UINT64_C(255), UINT64_C(256), UINT64_C(65536), UINT64_C(4294967295)}) {
        for(unsigned optional = 0; optional < 3; ++optional) {
            c::Inner inner;
            inner.value = value;
            if(optional) inner.flag = optional == 2;
            root_only<InnerAdapter>(inner);
            c::Outer outer;
            outer.inner = inner;
            if(optional) outer.flag = optional == 1;
            root_only<OuterAdapter>(outer);
        }
    }
    for(bool flag : {false, true}) {
        c::Collision collision;
        collision.sequence_extensions = flag;
        root_only<CollisionAdapter>(collision);
        collision.sequence_extensions_1 = !flag;
        root_only<CollisionAdapter>(collision);
    }
    REQUIRE(c::encode_empty({}).value().octets == std::vector<std::byte>{std::byte{0}});
    REQUIRE(c::encode_tail(c::Tail{true}).value().octets == std::vector<std::byte>{std::byte{0x80}});
    unknown_and_ownership();
    nested_budgets_and_oom();
    malformed_generated_decode();
    failure_hook_and_valueless();
}
