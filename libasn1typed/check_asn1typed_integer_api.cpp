#include "asn1typed_render_cpp.h"
#include <cstdlib>
using Renderer = int (*)(const asn1typed_module_t *, const char *, char **, char *, std::size_t);
int main() {
    Renderer functions[] = {asn1typed_render_cpp_owned_integer_types,
        asn1typed_render_cpp_owned_integer_mapping, asn1typed_render_cpp_owned_integer_codec,
        asn1typed_render_cpp_owned_value_types, asn1typed_render_cpp_owned_value_mapping,
        asn1typed_render_cpp_owned_value_codec};
    for(auto function : functions) {
        char* output = nullptr; char diagnostic[256] = {};
        if(function(nullptr, "integertests", &output, diagnostic, sizeof(diagnostic)) != -1 || output || !diagnostic[0]) std::abort();
    }
}
