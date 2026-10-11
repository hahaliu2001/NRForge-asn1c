#include "../libaper/runtime.hpp"
#include "runtime.hpp"
#include <cstdio>
int main() {
    nrforge::uper::Writer u;
    nrforge::aper::EncodeContext context;
    nrforge::aper::BitWriter a(context);
    if (!u.write_bits(7, 3) || !u.write_uint(65535, 0, 65535))
        return 1;
    if (!a.write_bit(true) || !a.write_bit(true) || !a.write_bit(true) ||
        !a.write_bounded_uint(65535, 0, 65535))
        return 1;
    if (u.cursor_bit() != 19 || a.cursor_bit() != 24)
        return 1;
    auto ue = u.finish();
    auto ae = a.finish();
    if (!ue || !ae || ue.value().octets == ae.value().octets)
        return 1;
    std::puts("APER/UPER independent headers, linkage and distinct alignment PASS");
}
