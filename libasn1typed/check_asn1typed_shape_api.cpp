#include "asn1typed_render_cpp.h"
#include <cstddef>
#include <cstdlib>
using Renderer = int (*)(const asn1typed_module_t *, const char *, char **, char *, std::size_t);
int main() {
    Renderer functions[] = {asn1typed_render_cpp_owned_shape_types,
        asn1typed_render_cpp_owned_shape_mapping, asn1typed_render_cpp_owned_shape_codec};
    for(auto function : functions) {
        char *output = nullptr; char diagnostic[256] = {};
        if(function(nullptr, "shapetest", &output, diagnostic, sizeof(diagnostic)) != -1 || output || !diagnostic[0]) std::abort();
    }
}
