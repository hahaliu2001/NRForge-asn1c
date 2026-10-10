#include "asn1typed_render_cpp.h"
#include <cstdio>
#include <initializer_list>
int main() {
    char *out = nullptr;
    char diagnostic[512];
    const auto calls = {asn1typed_render_cpp_owned_bit_types,
        asn1typed_render_cpp_owned_bit_mapping, asn1typed_render_cpp_owned_bit_codec};
    for(auto call : calls)
        if(call(nullptr, "test", &out, diagnostic, sizeof(diagnostic)) != -1 || out || !diagnostic[0]) return 1;
    std::puts("PASS C++ callers link all BIT C renderer APIs");
}
