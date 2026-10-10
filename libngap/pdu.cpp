#include "pdu.hpp"
#define NRFORGE_PDU_PROFILE ngap
#define NRFORGE_PDU_ROOT_COUNT 3
#define NRFORGE_PDU_EXTENSIBLE true
#define NRFORGE_PDU_REGISTRY ngap_registry_state
#define NRFORGE_PDU_ENCODE encode_ngap_pdu
#define NRFORGE_PDU_DECODE decode_ngap_pdu
#include "pdu_implementation.inc"
#undef NRFORGE_PDU_PROFILE
#undef NRFORGE_PDU_ROOT_COUNT
#undef NRFORGE_PDU_EXTENSIBLE
#undef NRFORGE_PDU_REGISTRY
#undef NRFORGE_PDU_ENCODE
#undef NRFORGE_PDU_DECODE
