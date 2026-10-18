"""Qualification-only R1 receive ledger; no SMF/AMF event or state mutation.

Run only on a successful complete native decode whose entire value has been
asserted against the independent oracle. This is not a production adapter.
"""
import ipaddress


class Refusal(Exception):
    def __init__(self, category):
        self.category = category


def refuse(category):
    raise Refusal(category)


def rows_for(desc):
    table = desc._const_tab._val
    return list(table.root) + list(table.ext or [])


def structural_policy(desc, value):
    """Walk oracle-described structure without discarding entries or additions."""
    if desc.TYPE == 'SEQUENCE OF':
        seen = set()
        for item in value:
            if isinstance(item, dict) and 'id' in item and 'criticality' in item:
                if item['id'] in seen:
                    refuse('invalid_transfer')
                seen.add(item['id'])
            structural_policy(desc._cont, item)
    elif desc.TYPE == 'SEQUENCE':
        if any(k.startswith('_ext_') for k in value):
            refuse('unsupported_profile')  # No unknown SEQUENCE addition is safe-listed.
        for name, item in value.items():
            child = desc._cont[name]
            if child.TYPE == 'OPEN_TYPE':
                row = next((r for r in rows_for(child) if r['id'] == value['id']), None)
                if row is None:
                    crit = value['criticality']
                    if crit == 'reject':
                        refuse('invalid_transfer')
                    if crit == 'notify':
                        refuse('unsupported_profile')
                    if crit != 'ignore' or not isinstance(item[1], bytes):
                        refuse('invalid_transfer')
                    # Unknown ignore-criticality IE/extension is safe at this
                    # protocol layer: retain bytes; do not project extra meaning.
                else:
                    if value['criticality'] != row['criticality']:
                        refuse('invalid_transfer')
                    structural_policy(row[child._const_tab_id], item[1])
            else:
                structural_policy(child, item)
    elif desc.TYPE == 'CHOICE':
        if value[0] not in desc._cont:
            refuse('unsupported_profile')
        structural_policy(desc._cont[value[0]], value[1])


def tunnel(value):
    if value[0] != 'gTPTunnel':
        refuse('unsupported_profile')
    item = value[1]
    address, width = item['transportLayerAddress']
    if width != 32 or len(item['gTP-TEID']) != 4 or not int.from_bytes(item['gTP-TEID'], 'big'):
        refuse('unsupported_profile')
    ip = ipaddress.IPv4Address(address)
    if ip.is_unspecified or ip.is_multicast or address == 0xffffffff:
        refuse('unsupported_profile')


# Exact R1 field allowlist. Unknown IE/extension payload is separately retained
# and handled by criticality; a known non-R1 optional is never blanket-ignored.
ALLOWED = {
    'protocolIEs', 'id', 'criticality', 'value', 'extensionValue', 'iE-Extensions',
    'pDUSessionAggregateMaximumBitRateDL', 'pDUSessionAggregateMaximumBitRateUL',
    'transportLayerAddress', 'gTP-TEID', 'qosFlowIdentifier',
    'qosFlowLevelQosParameters', 'qosCharacteristics', 'allocationAndRetentionPriority',
    'priorityLevelARP', 'pre-emptionCapability', 'pre-emptionVulnerability', 'fiveQI',
    'dLQosFlowPerTNLInformation', 'uPTransportLayerInformation', 'associatedQosFlowList', 'cause',
}


def supported_fields(value):
    if isinstance(value, dict):
        if set(value) - ALLOWED:
            refuse('unsupported_profile')
        for name, item in value.items():
            if name == 'iE-Extensions':
                # All known extension values are outside this R1 projection.
                if any(not row['extensionValue'][0].startswith('_unk_') for row in item):
                    refuse('unsupported_profile')
            supported_fields(item)
    elif isinstance(value, (list, tuple)):
        for item in value:
            supported_fields(item)


def classify(root, desc, value):
    try:
        structural_policy(desc, value)  # Presence/duplicates/criticality precede projection.
        supported_fields(value)
        if root == 'setup_request':
            entries = {row['id']: row for row in value['protocolIEs']}
            if not {130, 139, 134, 136} <= entries.keys():
                refuse('invalid_transfer')
            if any(k not in {130, 139, 134, 136} and
                   not v['value'][0].startswith('_unk_') for k, v in entries.items()):
                refuse('unsupported_profile')
            if entries[134]['value'][1] != 'ipv4':
                refuse('unsupported_profile')
            tunnel(entries[139]['value'][1])
            rates = entries[130]['value'][1]
            if any(not 1000 <= rates[k] <= 4000000000000 for k in (
                    'pDUSessionAggregateMaximumBitRateDL', 'pDUSessionAggregateMaximumBitRateUL')):
                refuse('unsupported_profile')
            flows = entries[136]['value'][1]
            if len({x['qosFlowIdentifier'] for x in flows}) != len(flows):
                refuse('invalid_transfer')
            if len(flows) != 1 or flows[0]['qosFlowIdentifier'] != 9:
                refuse('unsupported_profile')
            qos = flows[0]['qosFlowLevelQosParameters']['qosCharacteristics']
            if qos[0] != 'nonDynamic5QI' or qos[1]['fiveQI'] != 9:
                refuse('unsupported_profile')
        elif root == 'setup_response':
            primary = value['dLQosFlowPerTNLInformation']
            tunnel(primary['uPTransportLayerInformation'])
            flows = primary['associatedQosFlowList']
            if len({x['qosFlowIdentifier'] for x in flows}) != len(flows):
                refuse('invalid_transfer')
            if len(flows) != 1 or flows[0]['qosFlowIdentifier'] != 9:
                refuse('unsupported_profile')
        elif root == 'release_command' and value['cause'] != ('nas', 'normal-release'):
            refuse('unsupported_profile')
        return 'success'
    except Refusal as error:
        return error.category


def native_error(code, operation='decode'):
    if code in ('resource_limit', 'allocation_failure'):
        return 'resource_limit'
    if operation == 'decode' and code in (
            'truncated_input', 'nonzero_padding', 'trailing_data', 'constraint_violation'):
        return 'malformed_aper'
    if code == 'invalid_argument' or (operation == 'encode' and code == 'constraint_violation'):
        return 'local_argument_error'
    return 'internal_error'
