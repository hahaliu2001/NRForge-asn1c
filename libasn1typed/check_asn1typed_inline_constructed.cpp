#include <runtime.hpp>
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include "physical/types.hpp"
#include "physical/mapping.hpp"
#include "physical/codec.hpp"
#include <cstdio>
#include <cstdlib>
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); std::abort(); } } while(0)
int main() {
    inline_test::Root input{};
    input.first.value = true;
    std::get<0>(input.first.nested).value.bit = true;
    input.second.value = false;
    input.items.elements.push_back({true});
    input.selected.emplace<1>().value = false;
    auto encoded = inline_test::encode_root(input);
    REQUIRE(encoded);
    /* Independent bit model: 1, left=0, 1, 0, count=0, 1, two=1, 0. */
    REQUIRE(encoded.value().octets == std::vector<std::byte>{std::byte{0xa6}});
    auto decoded = inline_test::decode_root(encoded.value().octets);
    REQUIRE(decoded && decoded.value().first.value);
    REQUIRE(std::get<0>(decoded.value().first.nested).value.bit);
    REQUIRE(!decoded.value().second.value && decoded.value().items.elements.size() == 1);
    REQUIRE(decoded.value().items.elements[0].bit && decoded.value().selected.index() == 1);
    REQUIRE(!std::get<1>(decoded.value().selected).value);
    input.items.elements.clear();
    REQUIRE(!inline_test::encode_root(input));
    inline_test::Extended choice{};
    choice.emplace<0>().value = true;
    auto first = inline_test::encode_extended(choice);
    REQUIRE(first && first.value().octets == std::vector<std::byte>{std::byte{0x20}});
    choice.emplace<1>().value = true;
    auto second = inline_test::encode_extended(choice);
    REQUIRE(second && second.value().octets == std::vector<std::byte>{std::byte{0x60}});
    const std::vector<std::byte> extension{std::byte{0x80}};
    auto rejected_extension = inline_test::decode_extended(extension);
    REQUIRE(!rejected_extension && rejected_extension.error().code == nrforge::aper::ErrorCode::constraint_violation);
    REQUIRE(!inline_test::decode_extended({}));
    namespace c = inline_collection;
    c::InlineCollectionEntryInlineCollectionFlagRows entry{};
    entry.id = 1;
    entry.value.emplace<1>().value = true;
    c::InlineCollectionEntryInlineCollectionNestedRows outer{};
    outer.id = 2;
    outer.value.emplace<1>().value.elements.push_back(entry);
    c::InlineCollectionMessage message{};
    message.entries.elements.push_back(outer);
    auto nested = c::encode_inline_collection_message(message);
    REQUIRE(nested);
    /* Independent aligned IOC model: outer count/id/policy/open length,
     * then bounded list count, inner id/policy/open length/BOOLEAN. */
    const std::vector<std::byte> nested_bytes{std::byte{0}, std::byte{1}, std::byte{0}, std::byte{2},
        std::byte{0}, std::byte{6}, std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0}, std::byte{1}, std::byte{0x80}};
    REQUIRE(nested.value().octets == nested_bytes);
    auto recovered = c::decode_inline_collection_message(nested.value().octets);
    REQUIRE(recovered && recovered.value().entries.elements.size() == 1);
    const auto& list = std::get<1>(recovered.value().entries.elements[0].value).value;
    REQUIRE(list.elements.size() == 1 && list.elements[0].id == 1);
    REQUIRE(std::get<1>(list.elements[0].value).value);
    outer.id = 3;
    outer.value.emplace<2>().value = {std::byte{0x11}, std::byte{0x22}, std::byte{0x33}};
    message.entries.elements[0] = outer;
    auto constrained = c::encode_inline_collection_message(message);
    REQUIRE(constrained);
    const std::vector<std::byte> constrained_bytes{std::byte{0}, std::byte{1}, std::byte{0}, std::byte{3},
        std::byte{0}, std::byte{3}, std::byte{0x11}, std::byte{0x22}, std::byte{0x33}};
    REQUIRE(constrained.value().octets == constrained_bytes);
    auto constrained_decoded = c::decode_inline_collection_message(constrained_bytes);
    REQUIRE(constrained_decoded && std::get<2>(constrained_decoded.value().entries.elements[0].value).value.size() == 3);
    std::get<2>(message.entries.elements[0].value).value.pop_back();
    REQUIRE(!c::encode_inline_collection_message(message));
    return 0;
}
