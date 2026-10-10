#include "runtime.hpp"
#include "types.hpp"
#include "mapping.hpp"
#include "codec.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>

using namespace collectiontest;
static std::string hex(const std::vector<std::byte>& bytes) {
    std::ostringstream s;
    for(auto b : bytes) s << std::hex << std::setw(2) << std::setfill('0') << std::to_integer<unsigned>(b);
    return s.str();
}
static std::vector<std::byte> unhex(const std::string& text) {
    std::vector<std::byte> bytes;
    if(text == "-") return bytes;
    for(std::size_t i = 0; i < text.size(); i += 2)
        bytes.push_back(static_cast<std::byte>(std::stoul(text.substr(i, 2), nullptr, 16)));
    return bytes;
}
static Pair pair_value(std::size_t i, std::size_t pattern) {
    Pair v{}; v.value = (i * 17 + pattern) % 256;
    const auto presence = (i + pattern) % 3;
    if(presence) v.marker = presence == 2;
    return v;
}
template<class T> static T make_value(std::size_t count, std::size_t pattern) {
    T v{};
    using E = typename decltype(v.elements)::value_type;
    for(std::size_t i = 0; i < count; ++i) {
        if constexpr(std::is_same_v<E, bool>) v.elements.push_back((i + pattern) % 3 == 0);
        else if constexpr(std::is_same_v<E, Word>) v.elements.push_back((i * 17 + pattern) % 256);
        else if constexpr(std::is_same_v<E, Pair>) v.elements.push_back(pair_value(i, pattern));
        else if constexpr(std::is_same_v<E, EmptyElement>) v.elements.emplace_back();
        else if constexpr(std::is_same_v<E, Pick>) {
            if((i + pattern) % 2) v.elements.emplace_back(Pick_pair{pair_value(i, pattern)});
            else v.elements.emplace_back(Pick_flag{(i + pattern) % 3 == 0});
        } else if constexpr(std::is_same_v<E, Bits>) v.elements.push_back(make_value<Bits>((i + pattern) % 4, pattern + i));
        else if constexpr(std::is_same_v<E, ExtPair>) {
            ExtPair e{}; e.value = (i * 17 + pattern) % 256; v.elements.push_back(std::move(e));
        }
    }
    return v;
}
static std::string show(bool v) { return v ? "1" : "0"; }
static std::string show(Word v) { return std::to_string(v); }
static std::string show(const EmptyElement&) { return "{}"; }
static std::string show(const Pair& v) {
    return "(" + std::to_string(v.value) + ":" + (v.marker ? show(*v.marker) : "-") + ")";
}
static std::string show(const Pick& v) {
    return v.index() == 0 ? "f" + show(std::get<Pick_flag>(v).value) : "p" + show(std::get<Pick_pair>(v).value);
}
static std::string show(const ExtPair& v) {
    const auto& e = v.*ExtPair_aper::extension_data_member;
    std::string s = "(" + std::to_string(v.value) + ":" + std::to_string(e.received_bitmap_bit_count);
    for(const auto& r : e.unknown_additions) s += "," + std::to_string(r.addition_index) + "=" + hex(r.payload_octets);
    return s + ")";
}
template<class T> static std::string show(const T& v) {
    std::string s = "[";
    using E = typename decltype(v.elements)::value_type;
    for(std::size_t i = 0; i < v.elements.size(); ++i) {
        if(i) s += ",";
        if constexpr(std::is_same_v<E, bool>) s += show(static_cast<bool>(v.elements[i]));
        else s += show(v.elements[i]);
    }
    return s + "]";
}
template<class T, class Put, class Get>
static void run(const std::string& command, std::size_t residue, std::size_t count,
                std::size_t pattern, const std::vector<std::byte>& bytes, Put put, Get get) {
    auto decode = nrforge::aper::decode_complete<T>(bytes, {}, [&](nrforge::aper::FieldReader& f) {
        for(std::size_t i = 0; i < residue; ++i) {
            auto b = f.read_bit();
            if(!b) return nrforge::aper::Result<T>::failure(b.error());
            if(!b.value()) return nrforge::aper::Result<T>::failure({nrforge::aper::ErrorCode::invalid_argument, i});
        }
        return get(f);
    });
    if(!decode) {
        std::cout << "ERR:" << static_cast<unsigned>(decode.error().code) << ":" << decode.error().bit_offset << "\n";
        return;
    }
    if(command == "U") { std::cout << show(decode.value()) << "\n"; return; }
    auto expected = make_value<T>(count, pattern);
    if(show(decode.value()) != show(expected)) { std::cout << "BADVALUE\n"; return; }
    auto encode = nrforge::aper::encode_complete(expected, {}, [&](nrforge::aper::FieldWriter& f) {
        for(std::size_t i = 0; i < residue; ++i) {
            auto b = f.write_bit(true);
            if(!b) return b;
        }
        return put(f, expected);
    });
    if(!encode) { std::cout << "ENCERR\n"; return; }
    std::cout << hex(encode.value().octets) << " OK\n";
}
int main() {
    std::string command, type, wire;
    std::size_t residue, count, pattern;
    while(std::cin >> command >> type >> residue >> count >> pattern >> wire) {
        auto bytes = unhex(wire);
#define HANDLE(T) if(type == #T) { run<T>(command, residue, count, pattern, bytes, compound_codec::put_##T, compound_codec::get_##T); continue; }
        HANDLE(Bits) HANDLE(Words) HANDLE(Pairs) HANDLE(Picks) HANDLE(Nested) HANDLE(ZeroBitTriple)
        HANDLE(FixedThree) HANDLE(Empty) HANDLE(ShortEight) HANDLE(OctetEight)
        HANDLE(WideSixteen) HANDLE(FullSixteen) HANDLE(ExtPairs)
#undef HANDLE
        return 2;
    }
}
