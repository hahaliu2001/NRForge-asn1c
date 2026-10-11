#include "runtime.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
using namespace nrforge::uper;
template <class T> T require(Result<T> r) {
    if (!r)
        throw std::runtime_error("runtime error " +
                                 std::to_string(static_cast<int>(r.error().code)));
    return std::move(r).value();
}
void require(Result<void> r) {
    if (!r)
        throw std::runtime_error("runtime error");
}
std::vector<std::byte> unhex(const std::string &s) {
    std::vector<std::byte> v;
    if (s == "-")
        return v;
    for (std::size_t i = 0; i < s.size(); i += 2)
        v.push_back(std::byte{static_cast<unsigned char>(std::stoul(s.substr(i, 2), nullptr, 16))});
    return v;
}
std::string hex(std::span<const std::byte> v) {
    std::ostringstream o;
    for (auto b : v)
        o << std::hex << std::setfill('0') << std::setw(2) << std::to_integer<unsigned>(b);
    return v.empty() ? "-" : o.str();
}
int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        try {
            std::istringstream in(line);
            std::string op, kind, value, wire;
            unsigned offset;
            std::int64_t lo, hi;
            std::size_t count;
            bool general, ext;
            in >> op >> kind >> offset >> lo >> hi >> general >> ext >> count >> value >> wire;
            Size size{static_cast<std::size_t>(lo), static_cast<std::size_t>(hi), general, ext};
            if (op == "E") {
                Writer w;
                require(w.write_bits((std::uint64_t{1} << offset) - 1, offset));
                if (kind == "U")
                    require(w.write_uint(std::stoull(value), static_cast<std::uint64_t>(lo),
                                         static_cast<std::uint64_t>(hi)));
                else if (kind == "I")
                    require(w.write_int(std::stoll(value), lo, hi));
                else if (kind == "N")
                    require(w.write_unconstrained_int(std::stoll(value)));
                else if (kind == "X")
                    require(w.write_extensible_int(std::stoll(value), lo, hi));
                else if (kind == "V")
                    require(w.write_extensible_uint(std::stoull(value),
                                                    static_cast<std::uint64_t>(lo),
                                                    static_cast<std::uint64_t>(hi)));
                else if (kind == "A")
                    require(w.write_index({ext, std::stoull(value)}, static_cast<std::uint64_t>(hi),
                                          general));
                else if (kind == "O")
                    require(w.write_octets(unhex(value), size));
                else if (kind == "B")
                    require(w.write_bit_string({unhex(value), count}, size));
                else if (kind == "Q") {
                    require(w.write_bits(1, 1));
                    require(w.write_extension_bitmap({unhex(value), count}));
                    std::byte zero[] = {std::byte{0}};
                    require(w.write_open_type(zero));
                } else if (kind == "P") {
                    require(w.write_index({true, 0}, 1, true));
                    require(w.write_open_type(unhex(value)));
                } else
                    throw std::runtime_error("kind");
                auto e = require(w.finish());
                std::cout << hex(e.octets) << " " << e.semantic_bits << "\n";
            } else {
                auto bytes = unhex(wire);
                auto r = require(Reader::make(bytes));
                require(r.read_bits(offset));
                std::string got;
                if (kind == "U")
                    got = std::to_string(require(r.read_uint(static_cast<std::uint64_t>(lo),
                                                             static_cast<std::uint64_t>(hi))));
                else if (kind == "I")
                    got = std::to_string(require(r.read_int(lo, hi)));
                else if (kind == "N")
                    got = std::to_string(require(r.read_unconstrained_int()));
                else if (kind == "X")
                    got = std::to_string(require(r.read_extensible_int(lo, hi)));
                else if (kind == "V")
                    got = std::to_string(require(r.read_extensible_uint(
                        static_cast<std::uint64_t>(lo), static_cast<std::uint64_t>(hi))));
                else if (kind == "A") {
                    auto idx = require(r.read_index(static_cast<std::uint64_t>(hi), general));
                    if (idx.extension != ext)
                        throw std::runtime_error("index class");
                    got = std::to_string(idx.index);
                } else if (kind == "O")
                    got = hex(require(r.read_octets(size)));
                else if (kind == "B") {
                    auto b = require(r.read_bit_string(size));
                    if (b.bit_count != count)
                        throw std::runtime_error("bit count");
                    got = hex(b.octets);
                } else if (kind == "Q") {
                    if (require(r.read_bits(1)) != 1)
                        throw std::runtime_error("extension flag");
                    auto b = require(r.read_extension_bitmap());
                    if (b.bit_count != count)
                        throw std::runtime_error("bitmap count");
                    got = hex(b.octets);
                    auto payload = require(r.read_open_type());
                    if (payload != std::vector<std::byte>{std::byte{0}})
                        throw std::runtime_error("NULL payload");
                } else if (kind == "P") {
                    auto idx = require(r.read_index(1, true));
                    if (!idx.extension || idx.index)
                        throw std::runtime_error("choice index");
                    got = hex(require(r.read_open_type()));
                } else
                    throw std::runtime_error("kind");
                auto n = r.cursor_bit();
                require(r.finish());
                std::cout << got << " " << n << "\n";
            }
        } catch (const std::exception &e) {
            std::cout << "ERROR " << e.what() << "\n";
        }
    }
}
