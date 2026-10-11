// Intentionally instantiated once per protocol wrapper. Do not include directly.

#include "runtime.hpp"
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <variant>

namespace nrforge::NRFORGE_PDU_PROFILE {

enum class Role : unsigned { initiating, successful, unsuccessful };
enum class Criticality : unsigned { reject, ignore, notify };
enum class PduKind { invalid, typed, opaque_root, unknown_extension };

struct RootHeader {
    Role role;
    std::uint64_t procedure_code;
    Criticality received_criticality;
};
struct MessageInfo {
    Role role;
    std::uint64_t procedure_code;
    Criticality declared_criticality;
    std::string module;
    std::string message;
};
struct ProcedureInfo {
    std::uint64_t procedure_code;
    Criticality declared_criticality;
    std::array<bool, 3> payload_present;
};
struct OpaqueRoot { std::vector<std::byte> payload; };
struct UnknownExtension { std::uint64_t index; std::vector<std::byte> payload; };
// Owned envelope evidence supplies framing. Only these shared shapes
// are supported: NGAP/E1AP have three extensible roots; F1AP four nonextensible roots.
struct Framing { unsigned root_count; bool choice_is_extensible; };

namespace detail {
struct Table;
class Body {
public:
    virtual ~Body() = default;
    virtual std::unique_ptr<Body> clone() const = 0;
    virtual const std::type_info& value_type() const noexcept = 0;
};
template<class T>
class Model final : public Body {
public:
    template<class U> explicit Model(U&& v) : value(std::forward<U>(v)) {}
    std::unique_ptr<Body> clone() const override { return std::make_unique<Model>(value); }
    const std::type_info& value_type() const noexcept override { return typeid(T); }
    T value;
};
} // namespace detail

// A registration is supplied by generated adapters, not by sender-controlled
// wire data. Registry::create snapshots it and all descriptive strings. Type
// tokens must point to typeid(T) and typeid(detail::Model<T>) respectively.
struct Registration {
    Role role;
    std::uint64_t procedure_code;
    Criticality declared_criticality;
    const char* module;
    const char* message;
    const std::type_info* body_type;
    const std::type_info* model_type;
    aper::Result<std::unique_ptr<detail::Body>> (*decode)(aper::FieldReader&);
    aper::Result<void> (*encode)(aper::FieldWriter&, const detail::Body&);
};

class Registry;
class Pdu {
public:
    Pdu() noexcept = default; // Invalid until made/decoded; encode refuses it.
    Pdu(const Pdu&);
    Pdu& operator=(const Pdu&); // Deep BODY/payload copy, strong exception guarantee.
    Pdu(Pdu&&) noexcept;
    Pdu& operator=(Pdu&&) noexcept; // Source becomes explicitly invalid.
    ~Pdu() = default;
    void swap(Pdu&) noexcept;

    PduKind kind() const noexcept;
    const RootHeader* root_header() const noexcept;
    const MessageInfo* message_info() const noexcept;
    const OpaqueRoot* opaque_root() const noexcept;
    const UnknownExtension* unknown_extension() const noexcept;
    aper::Result<void> set_criticality(Criticality) noexcept; // Typed only.

    // Access checks the actual concrete Model<T>, never a numeric variant
    // ordinal or a caller-provided trait. Wrong BODY/invalid/opaque returns null.
    template<class T> const T* body_if() const noexcept {
        const auto* typed = std::get_if<Typed>(&content_);
        const auto* model = typed ? dynamic_cast<const detail::Model<T>*>(typed->body.get()) : nullptr;
        return model ? &model->value : nullptr;
    }
    template<class T> T* body_if() noexcept {
        auto* typed = std::get_if<Typed>(&content_);
        auto* model = typed ? dynamic_cast<detail::Model<T>*>(typed->body.get()) : nullptr;
        return model ? &model->value : nullptr;
    }

private:
    struct Typed { std::unique_ptr<detail::Body> body; };
    using Content = std::variant<std::monostate, Typed, OpaqueRoot, UnknownExtension>;
    static Content copy_content(const Content&);
    void invalidate() noexcept;
    std::shared_ptr<const detail::Table> table_; // Immutable metadata stays alive.
    std::size_t entry_ordinal_ = 0;
    std::optional<RootHeader> header_;
    Content content_;
    friend class Registry;
};

// Copies share immutable registry metadata; BODY values remain uniquely owned.
// Creation requires every declared (role, code) slot exactly once. Missing,
// duplicate, inconsistent or empty registries fail, not become opaque dispatch.
class Registry {
public:
    Registry() noexcept = default;
    static aper::Result<Registry> create(std::array<unsigned, 3> role_to_per_index,
        bool object_set_extensible, std::span<const ProcedureInfo>,
        std::span<const Registration>, Framing) noexcept;
    static aper::Result<Registry> create(std::array<unsigned, 3> mapping,
        bool extensible, std::span<const ProcedureInfo> procedures,
        std::span<const Registration> registrations) noexcept {
        return create(mapping, extensible, procedures, registrations,
            {NRFORGE_PDU_ROOT_COUNT, NRFORGE_PDU_EXTENSIBLE});
    }
    std::size_t message_count() const noexcept;
    const MessageInfo* message_info(std::size_t) const noexcept;

    template<class T> aper::Result<Pdu> make(T&& body,
            std::optional<Criticality> received = {}) const {
        using Value = std::remove_cvref_t<T>;
        const auto ordinal = find_type(typeid(Value));
        if(!ordinal || !expects_model(*ordinal, typeid(detail::Model<Value>)))
            return aper::Result<Pdu>::failure({aper::ErrorCode::invalid_argument, 0});
        const auto criticality = received.value_or(message_info(*ordinal)->declared_criticality);
        if(static_cast<unsigned>(criticality) > 2)
            return aper::Result<Pdu>::failure({aper::ErrorCode::constraint_violation, 0});
        try {
            return publish_typed(*ordinal, criticality,
                std::make_unique<detail::Model<Value>>(std::forward<T>(body)));
        } catch(const std::bad_alloc&) {
            return aper::Result<Pdu>::failure({aper::ErrorCode::allocation_failure, 0});
        } catch(const std::length_error&) {
            return aper::Result<Pdu>::failure({aper::ErrorCode::resource_limit, 0});
        }
    }
    aper::Result<void> encode_fields(aper::FieldWriter&, const Pdu&) const;
    aper::Result<Pdu> decode_fields(aper::FieldReader&) const;
    aper::Result<aper::CompleteEncoding> encode(const Pdu&, const aper::Limits& = {}) const;
    aper::Result<Pdu> decode(std::span<const std::byte>, const aper::Limits& = {}) const;

private:
    explicit Registry(std::shared_ptr<const detail::Table> t) noexcept : table_(std::move(t)) {}
    std::optional<std::size_t> find_type(const std::type_info&) const noexcept;
    bool expects_model(std::size_t, const std::type_info&) const noexcept;
    aper::Result<Pdu> publish_typed(std::size_t, Criticality,
        std::unique_ptr<detail::Body>) const;
    std::shared_ptr<const detail::Table> table_;
};

// Defined by the generated complete registry TU. Missing adapter TUs fail at
// link time. An invalid generated registry makes every public operation fail.
const aper::Result<Registry>& NRFORGE_PDU_REGISTRY() noexcept;
template<class T> aper::Result<Pdu> NRFORGE_PDU_MAKE(T&& body,
        std::optional<Criticality> criticality = {}) {
    const auto& registry = NRFORGE_PDU_REGISTRY();
    if(!registry) return aper::Result<Pdu>::failure(registry.error());
    return registry.value().make(std::forward<T>(body), criticality);
}
aper::Result<aper::CompleteEncoding> NRFORGE_PDU_ENCODE(const Pdu&, const aper::Limits& = {});
aper::Result<Pdu> NRFORGE_PDU_DECODE(std::span<const std::byte>, const aper::Limits& = {});

static_assert(std::is_nothrow_move_constructible_v<Pdu>);
static_assert(std::is_nothrow_move_assignable_v<Pdu>);
} // namespace nrforge::NRFORGE_PDU_PROFILE
