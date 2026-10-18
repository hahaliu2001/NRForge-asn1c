"""Current NGAP standalone root contract. Frozen explicit CONTAINING scope only."""
import re

VERSION = "0.3.0"
ROOTS = {'handover_command': 'HandoverCommandTransfer',
 'handover_preparation_unsuccessful': 'HandoverPreparationUnsuccessfulTransfer',
 'handover_request_acknowledge': 'HandoverRequestAcknowledgeTransfer',
 'handover_required': 'HandoverRequiredTransfer',
 'handover_resource_allocation_unsuccessful': 'HandoverResourceAllocationUnsuccessfulTransfer',
 'modify_confirm': 'PDUSessionResourceModifyConfirmTransfer',
 'modify_indication': 'PDUSessionResourceModifyIndicationTransfer',
 'modify_indication_unsuccessful': 'PDUSessionResourceModifyIndicationUnsuccessfulTransfer',
 'modify_request': 'PDUSessionResourceModifyRequestTransfer',
 'modify_response': 'PDUSessionResourceModifyResponseTransfer',
 'modify_unsuccessful': 'PDUSessionResourceModifyUnsuccessfulTransfer',
 'notify_released': 'PDUSessionResourceNotifyReleasedTransfer',
 'notify': 'PDUSessionResourceNotifyTransfer',
 'release_command': 'PDUSessionResourceReleaseCommandTransfer',
 'release_response': 'PDUSessionResourceReleaseResponseTransfer',
 'setup_request': 'PDUSessionResourceSetupRequestTransfer',
 'setup_response': 'PDUSessionResourceSetupResponseTransfer',
 'setup_unsuccessful': 'PDUSessionResourceSetupUnsuccessfulTransfer',
 'path_switch_request_acknowledge': 'PathSwitchRequestAcknowledgeTransfer',
 'path_switch_request_setup_failed': 'PathSwitchRequestSetupFailedTransfer',
 'path_switch_request': 'PathSwitchRequestTransfer',
 'path_switch_request_unsuccessful': 'PathSwitchRequestUnsuccessfulTransfer',
 'secondary_rat_data_usage_report': 'SecondaryRATDataUsageReportTransfer',
 'ue_context_resume_request': 'UEContextResumeRequestTransfer',
 'ue_context_resume_response': 'UEContextResumeResponseTransfer',
 'ue_context_suspend_request': 'UEContextSuspendRequestTransfer'}
LEGACY_ROOTS = {k: ROOTS[k] for k in ("setup_request", "setup_response", "release_command", "release_response")}

def cpp_type(name):
    # Same tokenizer as libasn1typed/asn1typed_name.c; module and type are
    # tokenized independently by the IOC renderer before concatenation.
    tokens = re.findall(r"[A-Z]+(?=[A-Z][a-z]|[0-9]|$)|[A-Z]?[a-z]+|[0-9]+", name)
    if "".join(tokens) != name.replace("-", ""):
        raise ValueError("unsafe ASN.1 root name")
    return "NgapIEs" + "".join(t[0].upper()+t[1:].lower() for t in tokens)
