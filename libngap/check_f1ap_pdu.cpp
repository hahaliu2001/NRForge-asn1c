#include "f1ap_pdu.hpp"
#include "pdu.hpp"
#include <cstdio>
#include <cstdlib>

namespace {
using namespace nrforge;
using namespace nrforge::f1ap;
static_assert(!std::is_same_v<ngap::Pdu, f1ap::Pdu>);
static_assert(!std::is_same_v<ngap::Role, f1ap::Role>);
static_assert(!std::is_same_v<ngap::detail::Body, f1ap::detail::Body>);
void require(bool v, int line) { if(!v) { std::fprintf(stderr,"F1AP registry:%d\n",line); std::abort(); } }
#define REQUIRE(x) require(static_cast<bool>(x),__LINE__)
template<unsigned N> struct Value { bool bit; };
template<class T> aper::Result<std::unique_ptr<detail::Body>> get(aper::FieldReader& f) {
    auto bit=f.read_bit();
    if(!bit) return aper::Result<std::unique_ptr<detail::Body>>::failure(bit.error());
    return aper::Result<std::unique_ptr<detail::Body>>::success(std::make_unique<detail::Model<T>>(T{bit.value()}));
}
template<class T> aper::Result<void> put(aper::FieldWriter& f,const detail::Body& v) {
    auto* model=dynamic_cast<const detail::Model<T>*>(&v);
    if(!model) return f.record_failure({aper::ErrorCode::invalid_state,f.cursor_bit()});
    return f.write_bit(model->value.bit);
}
template<unsigned N> Registration reg() {
    using T=Value<N>;
    return {static_cast<Role>(N),41,Criticality::ignore,"Fixture",N==0?"I":N==1?"S":"U",&typeid(T),&typeid(detail::Model<T>),get<T>,put<T>};
}
auto rows() { return std::array<ProcedureInfo,1>{{{41,Criticality::ignore,{true,true,true}}}}; }
auto entries() { return std::array<Registration,3>{{reg<0>(),reg<1>(),reg<2>()}}; }
auto registry() { return Registry::create({0,1,2},true,rows(),entries(),{4,false}); }
std::vector<std::byte> bytes(std::initializer_list<unsigned> data) {
    std::vector<std::byte> result; for(auto n:data) result.push_back(static_cast<std::byte>(n)); return result;
}
template<class T> void error(const aper::Result<T>& r,aper::ErrorCode code,std::size_t offset) {
    REQUIRE(!r);
    if(r.error().code!=code || r.error().bit_offset!=offset)
        std::fprintf(stderr,"expected %u@%zu got %u@%zu\n",static_cast<unsigned>(code),offset,static_cast<unsigned>(r.error().code),r.error().bit_offset);
    REQUIRE(r.error().code==code && r.error().bit_offset==offset);
}
template<unsigned N> void check() {
    auto pdu=make_f1ap_pdu(Value<N>{true}); REQUIRE(pdu);
    auto encoded=encode_f1ap_pdu(pdu.value()); REQUIRE(encoded);
    REQUIRE(encoded.value().octets==bytes({N<<6,41,0x40,1,0x80}));
    auto decoded=decode_f1ap_pdu(encoded.value().octets); REQUIRE(decoded);
    REQUIRE(decoded.value().template body_if<Value<N>>()->bit);
    REQUIRE(decoded.value().root_header()->role==static_cast<Role>(N));
    Pdu copied=decoded.value(); copied.body_if<Value<N>>()->bit=false;
    REQUIRE(decoded.value().template body_if<Value<N>>()->bit);
    Pdu moved=std::move(copied); REQUIRE(copied.kind()==PduKind::invalid); REQUIRE(moved.body_if<Value<N>>());
}
}
namespace nrforge::f1ap {
const aper::Result<Registry>& f1ap_registry_state() noexcept { static const auto state=registry(); return state; }
}
// Both independently typed protocol APIs can be included and linked together.
namespace nrforge::ngap {
const aper::Result<Registry>& ngap_registry_state() noexcept { static const auto state=aper::Result<Registry>::failure({aper::ErrorCode::invalid_state,0}); return state; }
}
int main() {
    check<0>(); check<1>(); check<2>();
    auto registry=::registry(); REQUIRE(registry); REQUIRE(registry.value().message_count()==3);
    for(auto data : {bytes({0xc0}),bytes({0xc0,255,0,0})}) {
        auto result=registry.value().decode(data); REQUIRE(!result);
        REQUIRE(result.error().code==nrforge::aper::ErrorCode::constraint_violation);
        REQUIRE(result.error().bit_offset==2);
    }
    auto unknown=registry.value().decode(bytes({0,255,0,1,0x55})); REQUIRE(unknown);
    REQUIRE(unknown.value().kind()==PduKind::opaque_root); REQUIRE(!registry.value().encode(unknown.value()));
    auto opaque_copy=unknown.value(); opaque_copy=unknown.value();
    REQUIRE(opaque_copy.opaque_root()->payload==bytes({0x55}));
    auto bound=registry.value().make(Value<0>{true}); REQUIRE(bound);
    auto independent=::registry(); REQUIRE(independent);
    error(independent.value().encode(bound.value()),aper::ErrorCode::invalid_argument,0);
    Registry shared=registry.value(); REQUIRE(shared.encode(bound.value()));
    auto borrowed=[] { return ::registry().value().make(Value<1>{true}); }(); REQUIRE(borrowed);
    REQUIRE(borrowed.value().message_info()->procedure_code==41);
    auto sparse_rows=rows(); sparse_rows[0].payload_present={true,false,false};
    auto sparse_entries=entries();
    auto sparse=Registry::create({0,1,2},true,sparse_rows,std::span(sparse_entries).first(1)); REQUIRE(sparse);
    auto absent=sparse.value().decode(bytes({0x40,41,0x40,1,0x55})); REQUIRE(absent);
    REQUIRE(absent.value().kind()==PduKind::opaque_root);
    auto closed=Registry::create({0,1,2},false,rows(),entries()); REQUIRE(closed);
    error(closed.value().decode(bytes({0,255,0,1,0x55})),aper::ErrorCode::constraint_violation,16);
    // Known malformed BODY stays an error, never receive-only opaque fallback.
    error(registry.value().decode(bytes({0,41,0x40,0})),aper::ErrorCode::constraint_violation,18);
    error(registry.value().decode(bytes({0,41,0xc0})),aper::ErrorCode::constraint_violation,16);
    aper::Limits limits; limits.max_input_octets=4;
    error(registry.value().decode(bytes({0,41,0x40,1,0x80}),limits),aper::ErrorCode::resource_limit,0);
    limits={}; limits.max_retained_unknown_payload_octets=0;
    error(registry.value().decode(bytes({0,255,0,1,0x55}),limits),aper::ErrorCode::resource_limit,18);
    REQUIRE(!Registry::create({0,0,2},true,rows(),entries(),{4,false}));
    REQUIRE(!Registry::create({0,1,2},true,rows(),entries(),{4,true}));
    REQUIRE(!Registry::create({0,1,2},true,rows(),entries(),{3,true}));
    // No assumption that unsupported choice-extension is the last root.
    auto reordered=Registry::create({2,0,3},true,rows(),entries(),{4,false}); REQUIRE(reordered);
    auto made=reordered.value().make(Value<0>{true}); REQUIRE(made);
    REQUIRE(reordered.value().encode(made.value()).value().octets==bytes({0x80,41,0x40,1,0x80}));
    auto rejected=reordered.value().decode(bytes({0x40})); REQUIRE(!rejected);
    REQUIRE(rejected.error().code==aper::ErrorCode::constraint_violation && rejected.error().bit_offset==2);
    auto registrations=entries();
    REQUIRE(!Registry::create({0,1,2},true,rows(),std::span(registrations).first(2),{4,false}));
    std::puts("PASS independent F1AP profile: three roles, no extension bit, fourth-root rejection and owned dispatch");
}
