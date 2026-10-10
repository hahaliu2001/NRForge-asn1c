// Shared implementation instantiated by pdu.cpp / f1ap_pdu.cpp.
#include <algorithm>

namespace nrforge::NRFORGE_PDU_PROFILE {
namespace detail {
struct Entry {
    MessageInfo info;
    const std::type_info* body_type;
    const std::type_info* model_type;
    aper::Result<std::unique_ptr<Body>> (*decode)(aper::FieldReader&);
    aper::Result<void> (*encode)(aper::FieldWriter&, const Body&);
};
struct Table {
    std::array<unsigned, 3> role_to_per_index;
    std::array<std::optional<Role>, 4> per_index_to_role;
    Framing framing;
    bool object_set_extensible;
    std::vector<ProcedureInfo> procedures;
    std::vector<Entry> entries;
};
} // namespace detail
namespace {
bool valid(Role role) noexcept { return static_cast<unsigned>(role) < 3; }
bool valid(Criticality criticality) noexcept { return static_cast<unsigned>(criticality) < 3; }
aper::Result<void> failure(aper::FieldWriter& f, aper::ErrorCode code) {
    return f.record_failure({code, f.cursor_bit()});
}
aper::Result<Pdu> failure(aper::FieldReader& f, aper::ErrorCode code) {
    const auto error = f.record_failure({code, f.cursor_bit()});
    return aper::Result<Pdu>::failure(error.error());
}
} // namespace

Pdu::Content Pdu::copy_content(const Content& source) {
    if(const auto* typed = std::get_if<Typed>(&source))
        return Typed{typed->body ? typed->body->clone() : nullptr};
    if(const auto* opaque = std::get_if<OpaqueRoot>(&source)) return *opaque;
    if(const auto* extension = std::get_if<UnknownExtension>(&source)) return *extension;
    return std::monostate{};
}
Pdu::Pdu(const Pdu& source) : table_(source.table_), entry_ordinal_(source.entry_ordinal_),
    header_(source.header_), content_(copy_content(source.content_)) {}
Pdu& Pdu::operator=(const Pdu& source) {
    if(this != &source) { Pdu copy(source); swap(copy); }
    return *this;
}
Pdu::Pdu(Pdu&& source) noexcept : table_(std::move(source.table_)),
    entry_ordinal_(source.entry_ordinal_), header_(source.header_), content_(std::move(source.content_)) {
    source.invalidate();
}
Pdu& Pdu::operator=(Pdu&& source) noexcept {
    if(this != &source) { Pdu moved(std::move(source)); swap(moved); }
    return *this;
}
void Pdu::swap(Pdu& other) noexcept {
    table_.swap(other.table_); std::swap(entry_ordinal_, other.entry_ordinal_);
    header_.swap(other.header_); content_.swap(other.content_);
}
void Pdu::invalidate() noexcept {
    table_.reset(); entry_ordinal_ = 0; header_.reset(); content_.emplace<std::monostate>();
}
PduKind Pdu::kind() const noexcept {
    if(std::holds_alternative<Typed>(content_)) return PduKind::typed;
    if(std::holds_alternative<OpaqueRoot>(content_)) return PduKind::opaque_root;
    if(std::holds_alternative<UnknownExtension>(content_)) return PduKind::unknown_extension;
    return PduKind::invalid;
}
const RootHeader* Pdu::root_header() const noexcept { return header_ ? &*header_ : nullptr; }
const MessageInfo* Pdu::message_info() const noexcept {
    if(kind() != PduKind::typed || !table_ || entry_ordinal_ >= table_->entries.size()) return nullptr;
    return &table_->entries[entry_ordinal_].info;
}
const OpaqueRoot* Pdu::opaque_root() const noexcept { return std::get_if<OpaqueRoot>(&content_); }
const UnknownExtension* Pdu::unknown_extension() const noexcept { return std::get_if<UnknownExtension>(&content_); }
aper::Result<void> Pdu::set_criticality(Criticality criticality) noexcept {
    if(kind() != PduKind::typed || !header_)
        return aper::Result<void>::failure({aper::ErrorCode::invalid_state, 0});
    if(!valid(criticality))
        return aper::Result<void>::failure({aper::ErrorCode::constraint_violation, 0});
    header_->received_criticality = criticality;
    return aper::Result<void>::success();
}

aper::Result<Registry> Registry::create(std::array<unsigned, 3> role_to_per_index,
        bool object_set_extensible, std::span<const ProcedureInfo> procedures,
        std::span<const Registration> registrations, Framing framing) noexcept {
    const auto bad = [] { return aper::Result<Registry>::failure({aper::ErrorCode::invalid_argument, 0}); };
    if(procedures.empty() || procedures.size() > 256 || registrations.empty() || registrations.size() > 768)
        return bad();
    if(framing.root_count != NRFORGE_PDU_ROOT_COUNT
        || framing.choice_is_extensible != NRFORGE_PDU_EXTENSIBLE) return bad();
    unsigned seen = 0;
    for(const auto index : role_to_per_index) {
        if(index >= framing.root_count || (seen & (1u << index))) return bad();
        seen |= 1u << index;
    }
    std::size_t expected = 0;
    for(std::size_t i = 0; i < procedures.size(); ++i) {
        const auto& row = procedures[i];
        if(row.procedure_code > 255 || !valid(row.declared_criticality) || !row.payload_present[0]) return bad();
        for(std::size_t j = 0; j < i; ++j)
            if(row.procedure_code == procedures[j].procedure_code) return bad();
        for(const auto present : row.payload_present) if(present) ++expected;
    }
    if(expected != registrations.size()) return bad();
    try {
        auto table = std::make_shared<detail::Table>();
        table->role_to_per_index = role_to_per_index;
        table->framing = framing;
        for(unsigned role = 0; role < 3; ++role)
            table->per_index_to_role[role_to_per_index[role]] = static_cast<Role>(role);
        table->object_set_extensible = object_set_extensible;
        table->procedures.assign(procedures.begin(), procedures.end());
        table->entries.reserve(registrations.size());
        for(const auto& registration : registrations) {
            if(!valid(registration.role) || registration.procedure_code > 255
                    || !valid(registration.declared_criticality)
                    || !registration.module || !*registration.module
                    || !registration.message || !*registration.message
                    || !registration.body_type || !registration.model_type
                    || !registration.decode || !registration.encode) return bad();
            const auto row = std::find_if(procedures.begin(), procedures.end(), [&](const auto& candidate) {
                return candidate.procedure_code == registration.procedure_code;
            });
            if(row == procedures.end() || !row->payload_present[static_cast<unsigned>(registration.role)]
                    || row->declared_criticality != registration.declared_criticality) return bad();
            for(const auto& previous : table->entries) {
                if((previous.info.role == registration.role
                        && previous.info.procedure_code == registration.procedure_code)
                    || *previous.body_type == *registration.body_type
                    || *previous.model_type == *registration.model_type
                    || (previous.info.module == registration.module && previous.info.message == registration.message))
                    return bad();
            }
            table->entries.push_back({{registration.role, registration.procedure_code,
                registration.declared_criticality, registration.module, registration.message},
                registration.body_type, registration.model_type, registration.decode, registration.encode});
        }
        return aper::Result<Registry>::success(Registry(std::move(table)));
    } catch(const std::bad_alloc&) {
        return aper::Result<Registry>::failure({aper::ErrorCode::allocation_failure, 0});
    } catch(const std::length_error&) {
        return aper::Result<Registry>::failure({aper::ErrorCode::resource_limit, 0});
    }
}
std::size_t Registry::message_count() const noexcept { return table_ ? table_->entries.size() : 0; }
const MessageInfo* Registry::message_info(std::size_t ordinal) const noexcept {
    return table_ && ordinal < table_->entries.size() ? &table_->entries[ordinal].info : nullptr;
}
std::optional<std::size_t> Registry::find_type(const std::type_info& type) const noexcept {
    if(table_) for(std::size_t i = 0; i < table_->entries.size(); ++i)
        if(*table_->entries[i].body_type == type) return i;
    return {};
}
bool Registry::expects_model(std::size_t ordinal, const std::type_info& type) const noexcept {
    return table_ && ordinal < table_->entries.size() && *table_->entries[ordinal].model_type == type;
}
aper::Result<Pdu> Registry::publish_typed(std::size_t ordinal, Criticality criticality,
        std::unique_ptr<detail::Body> body) const {
    if(!body || !expects_model(ordinal, typeid(*body))
            || *table_->entries[ordinal].body_type != body->value_type() || !valid(criticality))
        return aper::Result<Pdu>::failure({aper::ErrorCode::invalid_argument, 0});
    const auto& info = table_->entries[ordinal].info;
    Pdu pdu; pdu.table_ = table_; pdu.entry_ordinal_ = ordinal;
    pdu.header_ = RootHeader{info.role, info.procedure_code, criticality};
    pdu.content_ = Pdu::Typed{std::move(body)};
    return aper::Result<Pdu>::success(std::move(pdu));
}
aper::Result<void> Registry::encode_fields(aper::FieldWriter& f, const Pdu& pdu) const {
    if(!table_ || pdu.kind() == PduKind::invalid) return failure(f, aper::ErrorCode::invalid_state);
    if(pdu.kind() != PduKind::typed) return failure(f, aper::ErrorCode::constraint_violation);
    if(pdu.table_.get() != table_.get() || !pdu.header_ || pdu.entry_ordinal_ >= table_->entries.size())
        return failure(f, aper::ErrorCode::invalid_argument);
    const auto& entry = table_->entries[pdu.entry_ordinal_];
    const auto& typed = std::get<Pdu::Typed>(pdu.content_);
    const auto& header = *pdu.header_;
    if(!typed.body || !expects_model(pdu.entry_ordinal_, typeid(*typed.body))
            || *entry.body_type != typed.body->value_type()
            || header.role != entry.info.role || header.procedure_code != entry.info.procedure_code
            || !valid(header.received_criticality)) return failure(f, aper::ErrorCode::constraint_violation);
    auto status = f.write_enumerated({false, table_->role_to_per_index[static_cast<unsigned>(header.role)]}, table_->framing.root_count, table_->framing.choice_is_extensible);
    if(!status) return status;
    status = f.write_constrained_uint(header.procedure_code, 8);
    if(!status) return status;
    status = f.write_enumerated({false, static_cast<unsigned>(header.received_criticality)}, 3, false);
    if(!status) return status;
    return f.write_known_open_type([&](aper::FieldWriter& child) { return entry.encode(child, *typed.body); });
}
aper::Result<Pdu> Registry::decode_fields(aper::FieldReader& f) const {
    if(!table_) return failure(f, aper::ErrorCode::invalid_state);
    auto selector = f.read_enumerated(table_->framing.root_count, table_->framing.choice_is_extensible);
    if(!selector) return aper::Result<Pdu>::failure(selector.error());
    if(selector.value().is_extension) {
        auto payload = f.read_open_type_owned();
        if(!payload) return aper::Result<Pdu>::failure(payload.error());
        Pdu pdu; pdu.table_ = table_;
        pdu.content_ = UnknownExtension{selector.value().index, std::move(payload).value()};
        return aper::Result<Pdu>::success(std::move(pdu));
    }
    const auto selected_role = table_->per_index_to_role[static_cast<std::size_t>(selector.value().index)];
    // The additional root is not an outcome header. Reject before reading its
    // bytes as procedure/criticality or falling back to opaque outcome data.
    if(!selected_role) return failure(f, aper::ErrorCode::constraint_violation);
    const auto role = *selected_role;
    auto code = f.read_constrained_uint(8);
    if(!code) return aper::Result<Pdu>::failure(code.error());
    if(!table_->object_set_extensible && std::none_of(table_->procedures.begin(), table_->procedures.end(),
            [&](const auto& row) { return row.procedure_code == code.value(); }))
        return failure(f, aper::ErrorCode::constraint_violation);
    auto criticality = f.read_enumerated(3, false);
    if(!criticality) return aper::Result<Pdu>::failure(criticality.error());
    const auto received = static_cast<Criticality>(criticality.value().index);
    for(std::size_t i = 0; i < table_->entries.size(); ++i) {
        const auto& entry = table_->entries[i];
        if(entry.info.role != role || entry.info.procedure_code != code.value()) continue;
        auto payload = f.read_known_open_type<std::unique_ptr<detail::Body>>([&](aper::FieldReader& child) {
            auto body = entry.decode(child);
            if(!body) return body;
            if(!body.value() || !expects_model(i, typeid(*body.value()))
                    || *entry.body_type != body.value()->value_type()) {
                const auto invalid = child.record_failure({aper::ErrorCode::invalid_state, child.cursor_bit()});
                return aper::Result<std::unique_ptr<detail::Body>>::failure(invalid.error());
            }
            return body;
        });
        if(!payload) return aper::Result<Pdu>::failure(payload.error());
        return publish_typed(i, received, std::move(payload).value());
    }
    auto payload = f.read_open_type_owned();
    if(!payload) return aper::Result<Pdu>::failure(payload.error());
    Pdu pdu; pdu.table_ = table_; pdu.header_ = RootHeader{role, code.value(), received};
    pdu.content_ = OpaqueRoot{std::move(payload).value()};
    return aper::Result<Pdu>::success(std::move(pdu));
}
aper::Result<aper::CompleteEncoding> Registry::encode(const Pdu& pdu, const aper::Limits& limits) const {
    return aper::encode_complete(pdu, limits, [&](aper::FieldWriter& f) { return encode_fields(f, pdu); });
}
aper::Result<Pdu> Registry::decode(std::span<const std::byte> input, const aper::Limits& limits) const {
    return aper::decode_complete<Pdu>(input, limits, [&](aper::FieldReader& f) { return decode_fields(f); });
}
aper::Result<aper::CompleteEncoding> NRFORGE_PDU_ENCODE(const Pdu& pdu, const aper::Limits& limits) {
    const auto& registry = NRFORGE_PDU_REGISTRY();
    if(!registry) return aper::Result<aper::CompleteEncoding>::failure(registry.error());
    return registry.value().encode(pdu, limits);
}
aper::Result<Pdu> NRFORGE_PDU_DECODE(std::span<const std::byte> input, const aper::Limits& limits) {
    const auto& registry = NRFORGE_PDU_REGISTRY();
    if(!registry) return aper::Result<Pdu>::failure(registry.error());
    return registry.value().decode(input, limits);
}
} // namespace nrforge::NRFORGE_PDU_PROFILE
