#ifndef NRFORGE_E1AP_PDU_HPP
#define NRFORGE_E1AP_PDU_HPP
#define NRFORGE_PDU_PROFILE e1ap
#define NRFORGE_PDU_ROOT_COUNT 3
#define NRFORGE_PDU_EXTENSIBLE true
#define NRFORGE_PDU_REGISTRY e1ap_registry_state
#define NRFORGE_PDU_MAKE make_e1ap_pdu
#define NRFORGE_PDU_ENCODE encode_e1ap_pdu
#define NRFORGE_PDU_DECODE decode_e1ap_pdu
#include "pdu_declarations.inc"
#undef NRFORGE_PDU_PROFILE
#undef NRFORGE_PDU_ROOT_COUNT
#undef NRFORGE_PDU_EXTENSIBLE
#undef NRFORGE_PDU_REGISTRY
#undef NRFORGE_PDU_MAKE
#undef NRFORGE_PDU_ENCODE
#undef NRFORGE_PDU_DECODE
#endif
