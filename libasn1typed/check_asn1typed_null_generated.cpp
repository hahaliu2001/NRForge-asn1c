#include <runtime.hpp>
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include "renamed_types.hpp"
#include "renamed_mapping.hpp"
#include "renamed_codec.hpp"
#include "physical_types.hpp"
#include "physical_mapping.hpp"
#include "physical_codec.hpp"
#include <cstdlib>
#include <cstdio>
#include <type_traits>
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); std::abort(); } } while(0)
using namespace nulltest;
static_assert(std::is_same_v<Nothing, std::monostate>);
static_assert(Nothing_aper::bit_count == 0);
int main() {
    renamed_null::Envelope renamed{}; renamed.signal = true;
    auto re = renamed_null::encode_envelope(renamed);
    REQUIRE(re && re.value().octets == std::vector<std::byte>{std::byte{0x80}});
    auto rd = renamed_null::decode_envelope(re.value().octets); REQUIRE(rd && rd.value().signal);
    auto n = encode_nothing({}); REQUIRE(n && n.value().octets == std::vector<std::byte>{std::byte{0}});
    REQUIRE(n.value().empty_encoding_substitution && n.value().last_field_end_bit == 0);
    REQUIRE(decode_nothing(n.value().octets));
    for(bool present : {false,true}) for(bool marker : {false,true}) {
        Packet p{}; if(present) p.present = std::monostate{}; p.marker = marker;
        auto e = encode_packet(p); REQUIRE(e && e.value().octets.size() == 1);
        REQUIRE(e.value().octets[0] == std::byte((present ? 128 : 0) | (marker ? 64 : 0)));
        auto d = decode_packet(e.value().octets); REQUIRE(d && d.value().present.has_value() == present && d.value().marker == marker);
    }
    Select empty = Select_empty{};
    auto e = encode_select(empty); REQUIRE(e && e.value().octets == std::vector<std::byte>{std::byte{0}});
    auto d = decode_select(e.value().octets); REQUIRE(d && d.value().index() == 1);
    Select truth = Select_truth{true}; e = encode_select(truth);
    REQUIRE(e && e.value().octets == std::vector<std::byte>{std::byte{0xc0}});
    d = decode_select(e.value().octets); REQUIRE(d && d.value().index() == 0 && std::get<Select_truth>(d.value()).value);
    Trigger trigger = Trigger_coded{TriggerInlineCoded{TriggerInlineCoded::Known::high}};
    auto te = encode_trigger(trigger);
    REQUIRE(te && te.value().octets == std::vector<std::byte>{std::byte{0xa0}});
    auto td = decode_trigger(te.value().octets);
    REQUIRE(td && td.value().index() == 0 && std::get<TriggerInlineCoded::Known>(std::get<Trigger_coded>(td.value()).value.value) == TriggerInlineCoded::Known::high);
    trigger = Trigger_coded{TriggerInlineCoded{TriggerInlineCoded::Known::low}};
    te = encode_trigger(trigger);
    REQUIRE(te && te.value().octets == std::vector<std::byte>{std::byte{0x80}});
    td = decode_trigger(te.value().octets);
    REQUIRE(td && std::get<TriggerInlineCoded::Known>(std::get<Trigger_coded>(td.value()).value.value) == TriggerInlineCoded::Known::low);
    trigger = Trigger_coded{TriggerInlineCoded{TriggerInlineCoded::Known::future}};
    te = encode_trigger(trigger);
    REQUIRE((te && te.value().octets == std::vector<std::byte>{std::byte{0xc0},std::byte{0}}));
    td = decode_trigger(te.value().octets);
    REQUIRE(td && std::get<TriggerInlineCoded::Known>(std::get<Trigger_coded>(td.value()).value.value) == TriggerInlineCoded::Known::future);
    trigger = Trigger_coded{TriggerInlineCoded{TriggerInlineCoded::UnknownExtension{2}}};
    te = encode_trigger(trigger);
    REQUIRE((te && te.value().octets == std::vector<std::byte>{std::byte{0xc1},std::byte{0}}));
    td = decode_trigger(te.value().octets);
    REQUIRE(td && std::get<TriggerInlineCoded::UnknownExtension>(std::get<Trigger_coded>(td.value()).value.value).index == 2);
    auto bad = decode_nothing(std::vector<std::byte>{std::byte{0x10}});
    REQUIRE(!bad && bad.error().code == nrforge::aper::ErrorCode::nonzero_padding && bad.error().bit_offset == 3);
    iocnull::IocNullGenerationMessage msg{};
    iocnull::IocNullGenerationShapeEntryIocNullGenerationRows row{};
    row.number = 91; row.policy.value = iocnull::IocNullPayloadsPolicy::Known::reject;
    row.content = iocnull::IocNullGenerationShapeEntryIocNullGenerationRows_payload{};
    msg.entries.elements.push_back(row);
    auto physical = iocnull::encode_ioc_null_generation_message(msg);
    const std::vector<std::byte> pv{std::byte{0},std::byte{1},std::byte{0},std::byte{91},std::byte{0},std::byte{1},std::byte{0}};
    REQUIRE(physical && physical.value().octets == pv);
    auto pd = iocnull::decode_ioc_null_generation_message(pv);
    REQUIRE(pd && pd.value().entries.elements.size() == 1 && pd.value().entries.elements[0].content.index() == 1);
    std::puts("PASS NULL exact vectors and optional/choice storage order");
}
