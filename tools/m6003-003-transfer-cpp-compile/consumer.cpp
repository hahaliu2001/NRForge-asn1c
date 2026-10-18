#include <runtime.hpp>

#include "root_types.hpp"
#include "root_mapping.hpp"
#include "root_codec.hpp"

#include <cstddef>
#include <span>

#ifndef M6003_ROOT_TYPE
#error "M6003_ROOT_TYPE must name the generated root struct"
#endif
#ifndef M6003_ENCODE
#error "M6003_ENCODE must name the generated encode entry point"
#endif
#ifndef M6003_DECODE
#error "M6003_DECODE must name the generated decode entry point"
#endif

int main() {
    M6003_ROOT_TYPE root{};
    auto encoded = M6003_ENCODE(root);
    if(!encoded) return 10;

    const auto& octets = encoded.value().octets;
    auto decoded = M6003_DECODE(std::span<const std::byte>(octets.data(), octets.size()));
    return decoded ? 0 : 11;
}
