#include "e1ap_pdu.hpp"
#define NRFORGE_PDU_PROFILE e1ap
#define NRFORGE_PDU_ROOT_COUNT 3
#define NRFORGE_PDU_EXTENSIBLE true
#define NRFORGE_PDU_REGISTRY e1ap_registry_state
#define NRFORGE_PDU_ENCODE encode_e1ap_pdu
#define NRFORGE_PDU_DECODE decode_e1ap_pdu
#include "pdu_implementation.inc"
#undef NRFORGE_PDU_PROFILE
#undef NRFORGE_PDU_ROOT_COUNT
#undef NRFORGE_PDU_EXTENSIBLE
#undef NRFORGE_PDU_REGISTRY
#undef NRFORGE_PDU_ENCODE
#undef NRFORGE_PDU_DECODE
