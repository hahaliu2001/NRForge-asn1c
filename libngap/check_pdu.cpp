#include "pdu.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {
bool fail_allocation = false;
bool fail_model = false;
unsigned consumed = 0;
void require(bool value, int line) {
    if(!value) { std::fprintf(stderr, "check_pdu:%d\n", line); std::abort(); }
}
#define REQUIRE(x) require(static_cast<bool>(x), __LINE__)
using namespace nrforge;
using namespace nrforge::ngap;
struct Initiating { std::uint64_t value; };
struct Successful { std::vector<std::uint64_t> values; };
struct Unsuccessful { bool value; };
struct Extra { bool value; };
template<class T> aper::Result<std::unique_ptr<detail::Body>> get(aper::FieldReader& f) {
    T value{};
    if constexpr(std::is_same_v<T, Initiating> || std::is_same_v<T, Successful>) {
        auto read = f.read_aligned_u16_be();
        if(!read) return aper::Result<std::unique_ptr<detail::Body>>::failure(read.error());
        if constexpr(std::is_same_v<T, Initiating>) value.value = read.value();
        else value.values.push_back(read.value());
    } else {
        auto read = f.read_bit();
        if(!read) return aper::Result<std::unique_ptr<detail::Body>>::failure(read.error());
        value.value = read.value();
    }
    if(fail_model) { fail_model = false; fail_allocation = true; }
    return aper::Result<std::unique_ptr<detail::Body>>::success(
        std::make_unique<detail::Model<T>>(std::move(value)));
}
template<class T> aper::Result<void> put(aper::FieldWriter& f, const detail::Body& body) {
    auto* model = dynamic_cast<const detail::Model<T>*>(&body);
    if(!model) return f.record_failure({aper::ErrorCode::invalid_state, f.cursor_bit()});
    if constexpr(std::is_same_v<T, Initiating>) return f.write_aligned_u16_be(model->value.value);
    else if constexpr(std::is_same_v<T, Successful>) {
        if(model->value.values.size() != 1)
            return f.record_failure({aper::ErrorCode::constraint_violation, f.cursor_bit()});
        return f.write_aligned_u16_be(model->value.values[0]);
    } else return f.write_bit(model->value.value);
}
template<class T> Registration registration(Role role, std::uint64_t code, Criticality crit, const char* name) {
    return {role,code,crit,"Fixture",name,&typeid(T),&typeid(detail::Model<T>),get<T>,put<T>};
}
auto procedures() { return std::array<ProcedureInfo,2>{{
    {41,Criticality::ignore,{true,true,true}}, {0,Criticality::reject,{true,false,false}}}}; }
auto registrations() { return std::array<Registration,4>{{
    registration<Initiating>(Role::initiating,41,Criticality::ignore,"Initiating"),
    registration<Successful>(Role::successful,41,Criticality::ignore,"Successful"),
    registration<Unsuccessful>(Role::unsuccessful,41,Criticality::ignore,"Unsuccessful"),
    registration<Extra>(Role::initiating,0,Criticality::reject,"Extra")}}; }
aper::Result<Registry> registry(bool extensible = true) {
    return Registry::create({2,0,1},extensible,procedures(),registrations());
}
std::vector<std::byte> bytes(std::initializer_list<unsigned> values) {
    std::vector<std::byte> result;
    for(auto value:values) result.push_back(static_cast<std::byte>(value));
    return result;
}
template<class T> void error(const aper::Result<T>& result, aper::ErrorCode code, std::size_t offset) {
    REQUIRE(!result);
    if(result.error().code != code || result.error().bit_offset != offset)
        std::fprintf(stderr,"expected %d@%zu actual %d@%zu\n", static_cast<int>(code),offset,
            static_cast<int>(result.error().code),result.error().bit_offset);
    REQUIRE(result.error().code == code); REQUIRE(result.error().bit_offset == offset);
}
void typed_and_ownership() {
    auto reg = registry(); REQUIRE(reg); REQUIRE(reg.value().message_count() == 4);
    auto value = reg.value().make(Initiating{1}); REQUIRE(value);
    auto encoded = reg.value().encode(value.value()); REQUIRE(encoded);
    REQUIRE(encoded.value().octets == bytes({0x40,41,0x40,2,0,1}));
    auto decoded = reg.value().decode(encoded.value().octets); REQUIRE(decoded);
    REQUIRE(decoded.value().body_if<Initiating>()->value == 1);
    REQUIRE(!decoded.value().body_if<Successful>());
    REQUIRE(decoded.value().message_info()->message == "Initiating");
    REQUIRE(decoded.value().root_header()->role == Role::initiating);
    auto success = reg.value().make(Successful{{1}},Criticality::reject); REQUIRE(success);
    REQUIRE(reg.value().encode(success.value()).value().octets == bytes({0,41,0,2,0,1}));
    Pdu copy = success.value(); copy.body_if<Successful>()->values[0] = 7;
    REQUIRE(success.value().body_if<Successful>()->values[0] == 1);
    Pdu moved = std::move(copy); REQUIRE(copy.kind() == PduKind::invalid);
    error(reg.value().encode(copy),aper::ErrorCode::invalid_state,0);
    REQUIRE(moved.body_if<Successful>()->values[0] == 7);
    Registry shared = reg.value(); REQUIRE(shared.encode(moved));
    auto independent = registry(); REQUIRE(independent);
    error(independent.value().encode(moved),aper::ErrorCode::invalid_argument,0);
    auto failure = reg.value().make(Unsuccessful{true},Criticality::notify); REQUIRE(failure);
    REQUIRE(reg.value().encode(failure.value()).value().octets == bytes({0x20,41,0x80,1,0x80}));
    REQUIRE(failure.value().set_criticality(Criticality::reject));
    error(failure.value().set_criticality(static_cast<Criticality>(3)),aper::ErrorCode::constraint_violation,0);
    REQUIRE(failure.value().root_header()->received_criticality == Criticality::reject);
    Pdu survivor;
    { auto transient = registry(); survivor = std::move(transient.value().make(Initiating{9})).value(); }
    REQUIRE(survivor.message_info()->message == "Initiating");
    REQUIRE(survivor.body_if<Initiating>()->value == 9);
    error(reg.value().make(17),aper::ErrorCode::invalid_argument,0);
    fail_allocation = true; error(reg.value().make(Initiating{1}),aper::ErrorCode::allocation_failure,0);
    fail_allocation = true;
    try { moved = success.value(); REQUIRE(false); } catch(const std::bad_alloc&) {}
    REQUIRE(moved.body_if<Successful>()->values[0] == 7);
    REQUIRE(make_ngap_pdu(Initiating{1}));
    auto global = make_ngap_pdu(Initiating{1});
    REQUIRE(decode_ngap_pdu(encode_ngap_pdu(global.value()).value().octets));
}
void receive_and_failures() {
    auto reg = registry(); REQUIRE(reg);
    auto raw = bytes({0x40,250,0x40,1,0xa5});
    auto opaque = reg.value().decode(raw); REQUIRE(opaque);
    raw.back() = std::byte{};
    REQUIRE(opaque.value().opaque_root()->payload == bytes({0xa5}));
    REQUIRE(!opaque.value().message_info());
    error(reg.value().encode(opaque.value()),aper::ErrorCode::constraint_violation,0);
    auto absent = reg.value().decode(bytes({0x20,0,0x40,1,0xff}));
    REQUIRE(absent); REQUIRE(absent.value().kind() == PduKind::opaque_root);
    auto extension = reg.value().decode(bytes({0x80,1,0xaa})); REQUIRE(extension);
    REQUIRE(extension.value().unknown_extension()->index == 0);
    REQUIRE(extension.value().unknown_extension()->payload == bytes({0xaa}));
    error(reg.value().encode(extension.value()),aper::ErrorCode::constraint_violation,0);
    auto closed = registry(false); REQUIRE(closed);
    error(closed.value().decode(bytes({0x40,250,0x40,1,0xa5})),aper::ErrorCode::constraint_violation,16);
    error(reg.value().decode(bytes({0x60})),aper::ErrorCode::constraint_violation,0);
    error(reg.value().decode(bytes({0x40,41,0xc0})),aper::ErrorCode::constraint_violation,16);
    error(reg.value().decode(bytes({0x40,41,0x40,1,0})),aper::ErrorCode::truncated_input,40);
    auto bad = reg.value().make(Initiating{65536}); REQUIRE(bad);
    error(reg.value().encode(bad.value()),aper::ErrorCode::constraint_violation,18);
    fail_model = true;
    error(reg.value().decode(bytes({0x40,41,0x40,2,0,1})),aper::ErrorCode::allocation_failure,48);
    aper::Limits limits; limits.max_input_octets = 5;
    error(reg.value().decode(bytes({0x40,41,0x40,2,0,1}),limits),aper::ErrorCode::resource_limit,0);
}
aper::Result<std::unique_ptr<detail::Body>> wrong_body(aper::FieldReader& f) {
    auto value = f.read_bit();
    if(!value) return aper::Result<std::unique_ptr<detail::Body>>::failure(value.error());
    return aper::Result<std::unique_ptr<detail::Body>>::success(
        std::make_unique<detail::Model<Unsuccessful>>(Unsuccessful{value.value()}));
}
void registry_guards() {
    auto rows = procedures(); auto entries = registrations();
    REQUIRE(!Registry::create({2,0,1},true,rows,std::span(entries).first(3)));
    REQUIRE(!Registry::create({0,0,2},true,rows,entries));
    auto saved = entries[1]; entries[1] = entries[0];
    REQUIRE(!Registry::create({2,0,1},true,rows,entries)); entries[1] = saved;
    entries[1].decode = nullptr; REQUIRE(!Registry::create({2,0,1},true,rows,entries)); entries[1] = saved;
    entries[1].model_type = &typeid(detail::Model<Extra>);
    REQUIRE(!Registry::create({2,0,1},true,rows,entries)); entries[1] = saved;
    rows[0].procedure_code = 256; REQUIRE(!Registry::create({2,0,1},true,rows,entries)); rows = procedures();
    rows[0].payload_present[0] = false; REQUIRE(!Registry::create({2,0,1},true,rows,entries)); rows = procedures();
    fail_allocation = true; error(Registry::create({2,0,1},true,rows,entries),aper::ErrorCode::allocation_failure,0);
    // A trusted callback declaration cannot substitute a different concrete
    // BODY for the immutable identity. Check both actual value and model tokens.
    entries[0].model_type = &typeid(detail::Model<Unsuccessful>);
    entries[0].decode = wrong_body;
    entries[2].model_type = &typeid(int); // Keep declared model tokens distinct.
    auto forged = Registry::create({2,0,1},true,rows,entries); REQUIRE(forged);
    error(forged.value().make(Initiating{1}),aper::ErrorCode::invalid_argument,0);
    error(forged.value().decode(bytes({0x40,41,0x40,1,0x80})),aper::ErrorCode::invalid_state,33);
    entries = registrations();
    char name[] = "Snapshot"; entries[0].message = name;
    auto snapshot = Registry::create({2,0,1},true,rows,entries); REQUIRE(snapshot); name[0] = 'X';
    REQUIRE(snapshot.value().message_info(0)->message == "Snapshot");
}
}
void* operator new(std::size_t size) {
    if(fail_allocation) { fail_allocation = false; ++consumed; throw std::bad_alloc(); }
    if(void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
namespace nrforge::ngap {
const aper::Result<Registry>& ngap_registry_state() noexcept {
    static const auto result = registry(); return result;
}
}
int main() {
    typed_and_ownership(); receive_and_failures(); registry_guards();
    REQUIRE(consumed == 4);
    std::puts("PASS unified PDU identity, ownership, dispatch, receive-only and allocation guards");
}
