#include "f1ap_pdu.hpp"
#define NRFORGE_PDU_PROFILE f1ap
#define NRFORGE_PDU_ROOT_COUNT 4
#define NRFORGE_PDU_EXTENSIBLE false
#define NRFORGE_PDU_REGISTRY f1ap_registry_state
#define NRFORGE_PDU_ENCODE encode_f1ap_pdu
#define NRFORGE_PDU_DECODE decode_f1ap_pdu
#include "pdu_implementation.inc"
#undef NRFORGE_PDU_PROFILE
#undef NRFORGE_PDU_ROOT_COUNT
#undef NRFORGE_PDU_EXTENSIBLE
#undef NRFORGE_PDU_REGISTRY
#undef NRFORGE_PDU_ENCODE
#undef NRFORGE_PDU_DECODE
