#include "runtime.hpp"
#include "main_types.hpp"
#include "main_mapping.hpp"
#include "main_codec.hpp"
#include "main_adapters.hpp"
#include "empty_types.hpp"
#include "empty_mapping.hpp"
#include "empty_codec.hpp"
#include "empty_adapters.hpp"
#include "closed_types.hpp"
#include "closed_mapping.hpp"
#include "closed_codec.hpp"
#include "closed_adapters.hpp"
#include "ext_types.hpp"
#include "ext_mapping.hpp"
#include "ext_codec.hpp"
#include "ext_adapters.hpp"
#include "empty_ext_types.hpp"
#include "empty_ext_mapping.hpp"
#include "empty_ext_codec.hpp"
#include "empty_ext_adapters.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
namespace g = foo::nrforge::main;
static std::vector<std::byte> unhex(const std::string& s) {
    if(s.size() % 2) std::exit(2);
    std::vector<std::byte> out;
    for(std::size_t i = 0; i < s.size(); i += 2) out.push_back(static_cast<std::byte>(std::stoul(s.substr(i, 2), nullptr, 16)));
    return out;
}
static std::string hex(std::span<const std::byte> bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for(const auto b : bytes) { const auto n = std::to_integer<unsigned>(b); out += digits[n >> 4]; out += digits[n & 15U]; }
    return out;
}
template<class Entries> static void dump(const Entries& values) {
    for(const auto& entry : values.elements) {
        if constexpr(requires { entry.number; }) std::cout << entry.number << ':' << static_cast<unsigned>(entry.policy.value) << ':';
        else std::cout << entry.tag << ':' << static_cast<unsigned>(entry.received_policy.value) << ':';
        const auto show = [](const auto& payload) {
            if constexpr(requires { payload.payload; }) std::cout << "unknown:" << hex(payload.payload);
            else if constexpr(std::is_same_v<std::remove_cvref_t<decltype(payload.value)>, bool>) std::cout << "flag:" << (payload.value ? 1 : 0);
            else if constexpr(requires { payload.value.elements; }) { std::cout << "words:"; for(const auto word : payload.value.elements) std::cout << word << ","; }
            else if constexpr(requires { payload.value.marker; }) std::cout << "pair:" << payload.value.value << "," << (payload.value.marker ? (*payload.value.marker ? "1" : "0") : "absent");
            else std::cout << "word:" << payload.value;
        };
        if constexpr(requires { entry.content; }) std::visit(show, entry.content);
        else std::visit(show, entry.extension_content);
        std::cout << ';';
    }
    std::cout << '\n';
}
int main(int argc, char** argv) {
    if(argc != 4) return 2;
    const std::string mode = argv[1], type = argv[2];
    std::string argument = argv[3]; if(argument == "-") std::getline(std::cin, argument);
    if((mode == "decode" || mode == "refuse") && type == "dispatch") {
        auto input = unhex(argument);
        auto result = g::decode_message(input);
        if(!result) { std::cout << "error:" << static_cast<unsigned>(result.error().code) << ':' << result.error().bit_offset << '\n'; return 0; }
        std::fill(input.begin(), input.end(), std::byte{0});
        if(mode == "refuse") { auto output = g::encode_message(result.value()); if(output) return 3; std::cout << "error:" << static_cast<unsigned>(output.error().code) << ':' << output.error().bit_offset << '\n'; return 0; }
        dump(result.value().entries); return 0;
    }
    if(mode == "decode" && type == "empty") {
        auto result = foo::nrforge::empty::decode_message(unhex(argument));
        if(!result) { std::cout << "error:" << static_cast<unsigned>(result.error().code) << ':' << result.error().bit_offset << '\n'; return 0; }
        dump(result.value().items); return 0;
    }
    if(mode == "decode" && type == "extension") {
        auto result = foo::nrforge::ext::decode_message(unhex(argument));
        if(!result) { std::cout << "error:" << static_cast<unsigned>(result.error().code) << ':' << result.error().bit_offset << '\n'; return 0; }
        dump(result.value().additions); return 0;
    }
    if(mode == "decode" && type == "empty_extension") {
        auto result = foo::nrforge::empty_ext::decode_message(unhex(argument));
        if(!result) { std::cout << "error:" << static_cast<unsigned>(result.error().code) << ':' << result.error().bit_offset << '\n'; return 0; }
        dump(result.value().additions); return 0;
    }
    if(mode == "decode" && type == "closed") {
        auto result = foo::nrforge::closed::decode_message(unhex(argument));
        if(!result) { std::cout << "error:" << static_cast<unsigned>(result.error().code) << ':' << result.error().bit_offset << '\n'; return 0; }
        dump(result.value().entries); return 0;
    }
    if(mode == "encode" && type == "dispatch") {
        g::Message value;
        std::istringstream records(argument); std::string record;
        while(std::getline(records, record, ';')) {
            if(record.empty()) continue;
            std::istringstream fields(record); std::string id, policy, payload;
            std::getline(fields, id, ':'); std::getline(fields, policy, ':'); std::getline(fields, payload, ':');
            g::Entry entry;
            entry.number = std::stoull(id);
            entry.policy.value = static_cast<g::CppPayloadTypesPolicy::Known>(std::stoul(policy));
            if(entry.number == 91) entry.content = g::EntryMapping::wrapper_0{payload == "1"};
            else if(entry.number == 42) entry.content = g::EntryMapping::wrapper_2{payload == "1"};
            else if(entry.number == 7) entry.content = g::EntryMapping::wrapper_1{std::stoull(payload)};
            else if(entry.number == 50) {
                g::CppPayloadTypesPair pair;
                std::istringstream input(payload); std::string word, marker;
                std::getline(input, word, ','); std::getline(input, marker, ',');
                pair.value = std::stoull(word); if(marker != "absent") pair.marker = marker == "1";
                entry.content = g::EntryMapping::wrapper_3{pair};
            } else if(entry.number == 80) {
                g::CppPayloadTypesWordList words;
                std::istringstream input(payload); std::string word;
                while(std::getline(input, word, ',')) if(!word.empty()) words.elements.push_back(std::stoull(word));
                entry.content = g::EntryMapping::wrapper_4{words};
            } else return 2;
            value.entries.elements.push_back(std::move(entry));
        }
        auto result = g::encode_message(value);
        if(!result) { std::cout << "error:" << static_cast<unsigned>(result.error().code) << ':' << result.error().bit_offset << '\n'; return 0; }
        std::cout << hex(result.value().octets) << '\n'; return 0;
    }
    return 2;
}
