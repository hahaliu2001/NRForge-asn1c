// Test adapter only: expected bytes and error positions live in the Python oracle.
#include <runtime.hpp>
#include "main/types.hpp"
#include "main/mapping.hpp"
#include "main/codec.hpp"
#include "reversed/types.hpp"
#include "reversed/mapping.hpp"
#include "reversed/codec.hpp"
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using ::nrforge::aper::CompleteEncoding;
using ::nrforge::aper::Error;
using ::nrforge::aper::ErrorCode;
using ::nrforge::aper::Limits;
using ::nrforge::aper::Result;

#define ADAPTER(NAME, NS) \
struct NAME { \
    using Selection = NS::Selection; using Packet = NS::Packet; \
    using Flag = NS::Selection_flag; using Count = NS::Selection_count; \
    static auto ec(std::uint64_t v, const Limits& l) { return NS::encode_count(v, l); } \
    static auto es(const Selection& v, const Limits& l) { return NS::encode_selection(v, l); } \
    static auto ep(const Packet& v, const Limits& l) { return NS::encode_packet(v, l); } \
    static auto dc(const std::vector<std::byte>& v, const Limits& l) { return NS::decode_count(v, l); } \
    static auto ds(const std::vector<std::byte>& v, const Limits& l) { return NS::decode_selection(v, l); } \
    static auto dp(const std::vector<std::byte>& v, const Limits& l) { return NS::decode_packet(v, l); } \
};
ADAPTER(Main, ::qualification::main)
ADAPTER(Reversed, ::qualification::reversed)
#undef ADAPTER

static const char* error_name(ErrorCode c) {
    switch(c) {
    case ErrorCode::invalid_argument: return "invalid_argument";
    case ErrorCode::constraint_violation: return "constraint_violation";
    case ErrorCode::truncated_input: return "truncated_input";
    case ErrorCode::nonzero_padding: return "nonzero_padding";
    case ErrorCode::trailing_data: return "trailing_data";
    case ErrorCode::resource_limit: return "resource_limit";
    case ErrorCode::allocation_failure: return "allocation_failure";
    case ErrorCode::invalid_state: return "invalid_state";
    }
    throw std::runtime_error("unknown ErrorCode");
}

static void print_error(const Error& e) {
    std::cout << "ERR " << error_name(e.code) << ' ' << e.bit_offset << '\n';
}

static std::vector<std::byte> unhex(const std::string& s) {
    if(s == "-") return {};
    if(s.size() % 2) throw std::runtime_error("odd hex length");
    std::vector<std::byte> v;
    for(std::size_t i = 0; i < s.size(); i += 2) {
        auto n = std::stoul(s.substr(i, 2), nullptr, 16);
        v.push_back(static_cast<std::byte>(n));
    }
    return v;
}

static std::string hex(const std::vector<std::byte>& v) {
    constexpr char digits[] = "0123456789abcdef";
    std::string s;
    for(auto b : v) {
        auto n = std::to_integer<unsigned>(b);
        s += digits[n >> 4]; s += digits[n & 15];
    }
    return s;
}

template<class A>
static typename A::Selection selection(unsigned branch, std::uint64_t value) {
    if(branch == 0) return typename A::Flag{value != 0};
    return typename A::Count{value};
}

template<class A>
static void print_selection(const typename A::Selection& v) {
    if(auto p = std::get_if<typename A::Flag>(&v)) std::cout << "0 " << p->value;
    else std::cout << "1 " << std::get<typename A::Count>(v).value;
    std::cout << ' ' << v.index();
}

template<class A>
static void decode(const std::string& type, const std::vector<std::byte>& wire,
                   const Limits& limits) {
    if(type == "C") {
        auto d = A::dc(wire, limits);
        if(!d) return print_error(d.error());
        std::cout << "OK " << d.value() << '\n';
    } else if(type == "S") {
        auto d = A::ds(wire, limits);
        if(!d) return print_error(d.error());
        std::cout << "OK "; print_selection<A>(d.value()); std::cout << '\n';
    } else if(type == "P") {
        auto d = A::dp(wire, limits);
        if(!d) return print_error(d.error());
        auto& p = d.value();
        const int presence = p.enabled ? (*p.enabled ? 2 : 1) : 0;
        std::cout << "OK " << p.count << ' ' << presence << ' ';
        print_selection<A>(p.selection); std::cout << '\n';
    } else throw std::runtime_error("unknown type");
}

template<class A>
static void run(char command, const std::string& type, std::uint64_t count,
                int presence, unsigned branch, std::uint64_t payload,
                const std::vector<std::byte>& wire, const Limits& limits) {
    if(command == 'D') return decode<A>(type, wire, limits);
    auto encoded = [&]() -> Result<CompleteEncoding> {
        if(type == "C") return A::ec(count, limits);
        if(type == "S") return A::es(selection<A>(branch, payload), limits);
        if(type != "P") throw std::runtime_error("unknown type");
        typename A::Packet p{}; p.count = count;
        if(presence) p.enabled = presence == 2;
        p.selection = selection<A>(branch, payload);
        return A::ep(p, limits);
    }();
    if(!encoded) return print_error(encoded.error());
    const auto& e = encoded.value();
    std::cout << "ENC " << hex(e.octets) << ' ' << e.last_field_end_bit << ' '
              << static_cast<unsigned>(e.final_padding_bits) << ' '
              << e.empty_encoding_substitution << ' ' << e.complete_encoding_bits
              << ' ' << e.octet_count << '\n';
    // Decode the independent supplied wire, never the local encoder output.
    if(command == 'V') decode<A>(type, wire, limits);
}

int main() {
    std::string line;
    while(std::getline(std::cin, line)) {
        std::istringstream in(line);
        char command; unsigned ns, branch; int presence;
        std::string type, wire; std::uint64_t count, payload;
        Limits limits{};
        if(!(in >> command >> ns >> type >> count >> presence >> branch >> payload
                >> wire >> limits.max_input_octets >> limits.max_output_octets
                >> limits.max_wire_bits) || ns > 1 || branch > 1 || presence < 0 || presence > 2)
            throw std::runtime_error("invalid qualification request");
        if(ns == 0) run<Main>(command, type, count, presence, branch, payload, unhex(wire), limits);
        else run<Reversed>(command, type, count, presence, branch, payload, unhex(wire), limits);
    }
}
