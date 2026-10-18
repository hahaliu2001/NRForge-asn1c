"""Finite oracle values. No APER is encoded by this module."""
from copy import deepcopy
from itertools import product

ROOTS = {
    'setup_request': 'PDUSessionResourceSetupRequestTransfer',
    'setup_response': 'PDUSessionResourceSetupResponseTransfer',
    'release_command': 'PDUSessionResourceReleaseCommandTransfer',
    'release_response': 'PDUSessionResourceReleaseResponseTransfer',
}


def tunnel(address=0x7f000032, teid=1, width=32):
    return ('gTPTunnel', {'transportLayerAddress': (address, width),
                         'gTP-TEID': teid.to_bytes(4, 'big')})


def flow(arp=1, capability='shall-not-trigger-pre-emption',
         vulnerability='not-pre-emptable'):
    return {'qosFlowIdentifier': 9, 'qosFlowLevelQosParameters': {
        'qosCharacteristics': ('nonDynamic5QI', {'fiveQI': 9}),
        'allocationAndRetentionPriority': {
            'priorityLevelARP': arp, 'pre-emptionCapability': capability,
            'pre-emptionVulnerability': vulnerability}}}


def ie(number, typename, value, criticality='reject'):
    return {'id': number, 'criticality': criticality, 'value': (typename, value)}


def request(address=0x7f000032, teid=1, rate=1000, arp=1,
            capability='shall-not-trigger-pre-emption', vulnerability='not-pre-emptable'):
    return {'protocolIEs': [
        ie(130, 'PDUSessionAggregateMaximumBitRate', {
            'pDUSessionAggregateMaximumBitRateDL': rate,
            'pDUSessionAggregateMaximumBitRateUL': rate}),
        ie(139, 'UPTransportLayerInformation', tunnel(address, teid)),
        ie(134, 'PDUSessionType', 'ipv4'),
        ie(136, 'QosFlowSetupRequestList', [flow(arp, capability, vulnerability)])]}


def response(address=0x7f000032, teid=1, width=32, qfis=(9,)):
    return {'dLQosFlowPerTNLInformation': {
        'uPTransportLayerInformation': tunnel(address, teid, width),
        'associatedQosFlowList': [{'qosFlowIdentifier': qfi} for qfi in qfis]}}


def build_cases():
    rows = []

    def add(root, name, value, policy='success', encode=True, category='valid'):
        rows.append(dict(root=root, name=name, value=value, policy=policy,
                         encode=encode, category=category))

    # Independent parameters avoid a large Cartesian campaign.
    for addr, teid in ((0x7f000032, 1), (0x0a010203, 0xffffffff)):
        add('setup_request', f'tunnel-{addr}-{teid}', request(addr, teid))
        add('setup_response', f'tunnel-{addr}-{teid}', response(addr, teid))
    # Exact NAS-derived rates: unit 5/6 boundary and maximum supported bit rate.
    for rate in (1000, 256000, 1000000, 4000000000000):
        add('setup_request', f'rate-{rate}', request(rate=rate))
    for rate in (0, 999, 4000000000001):
        add('setup_request', f'unsupported-rate-{rate}', request(rate=rate), 'unsupported_profile')
    for arp, cap, vul in product((1, 15),
            ('shall-not-trigger-pre-emption', 'may-trigger-pre-emption'),
            ('not-pre-emptable', 'pre-emptable')):
        add('setup_request', f'arp-{arp}-{cap}-{vul}',
            request(arp=arp, capability=cap, vulnerability=vul))
    add('release_command', 'normal', {'cause': ('nas', 'normal-release')})
    for family, reason in (('radioNetwork', 'unspecified'), ('transport', 'unspecified'),
                           ('protocol', 'unspecified'), ('misc', 'unspecified')):
        add('release_command', 'cause-'+family, {'cause': (family, reason)}, 'unsupported_profile')
    add('release_response', 'empty', {})
    base = request()
    add('setup_request', 'asn1-empty-container', {'protocolIEs': []}, 'invalid_transfer')
    reordered = deepcopy(base); reordered['protocolIEs'].reverse()
    add('setup_request', 'reordered', reordered)
    duplicate = deepcopy(base); duplicate['protocolIEs'].append(deepcopy(base['protocolIEs'][0]))
    add('setup_request', 'duplicate', duplicate, 'invalid_transfer')
    for number in (130, 139, 134, 136):
        missing = deepcopy(base)
        missing['protocolIEs'] = [x for x in missing['protocolIEs'] if x['id'] != number]
        add('setup_request', f'missing-{number}', missing, 'invalid_transfer')
    mismatch = deepcopy(base); mismatch['protocolIEs'][0]['criticality'] = 'ignore'
    add('setup_request', 'criticality-mismatch', mismatch, 'invalid_transfer')
    wrong_fiveqi = deepcopy(base)
    wrong_fiveqi['protocolIEs'][3]['value'][1][0]['qosFlowLevelQosParameters']['qosCharacteristics'][1]['fiveQI'] = 8
    add('setup_request', 'wrong-fiveqi', wrong_fiveqi, 'unsupported_profile')
    known_optional = deepcopy(base)
    known_optional['protocolIEs'].append(ie(129, 'NetworkInstance', 1))
    add('setup_request', 'known-network-instance', known_optional, 'unsupported_profile')
    for root in ROOTS:
        for criticality, policy in (('reject', 'invalid_transfer'), ('ignore', 'success'),
                                     ('notify', 'unsupported_profile')):
            value = deepcopy(base) if root == 'setup_request' else (
                response() if root == 'setup_response' else (
                {'cause': ('nas', 'normal-release')} if root == 'release_command' else {}))
            entry = {'id': 65535, 'criticality': criticality,
                     ('value' if root == 'setup_request' else 'extensionValue'): ('_unk_004', b'\x80\x01')}
            value.setdefault('protocolIEs' if root == 'setup_request' else 'iE-Extensions', []).append(entry)
            add(root, 'unknown-'+criticality, value, policy, False, 'unknown_open')
        value = deepcopy(base) if root == 'setup_request' else (
            response() if root == 'setup_response' else (
            {'cause': ('nas', 'normal-release')} if root == 'release_command' else {}))
        field = 'protocolIEs' if root == 'setup_request' else 'iE-Extensions'
        entry = {'id': 65535, 'criticality': 'ignore',
                 ('value' if root == 'setup_request' else 'extensionValue'): ('_unk_004', b'\x80')}
        value.setdefault(field, []).extend([entry, deepcopy(entry)])
        add(root, 'duplicate-unknown-extension', value, 'invalid_transfer', False, 'unknown_open')
        ordered = deepcopy(base) if root == 'setup_request' else (
            response() if root == 'setup_response' else (
            {'cause': ('nas', 'normal-release')} if root == 'release_command' else {}))
        other = deepcopy(entry); other['id'] = 65534
        other['value' if root == 'setup_request' else 'extensionValue'] = ('_unk_004', b'\x01\x02')
        ordered.setdefault(field, []).extend([other, deepcopy(entry)])
        add(root, 'unknown-order-forward', deepcopy(ordered), 'success', False, 'unknown_open')
        if root == 'setup_request':
            ordered[field][-2:] = list(reversed(ordered[field][-2:]))
        else:
            ordered[field].reverse()
        add(root, 'unknown-order-reverse', ordered, 'success', False, 'unknown_open')
    for width, addr in ((128, 1), (160, 1)):
        add('setup_response', f'address-width-{width}', response(addr, width=width), 'unsupported_profile')
    add('setup_response', 'zero-teid', response(teid=0), 'unsupported_profile')
    for address in (0, 0xe0000001, 0xffffffff):
        add('setup_response', f'unsupported-ipv4-{address}', response(address), 'unsupported_profile')
    add('setup_response', 'wrong-qfi', response(qfis=(8,)), 'unsupported_profile')
    add('setup_response', 'duplicate-qfi', response(qfis=(9, 9)), 'invalid_transfer')
    add('setup_response', 'extra-qfi', response(qfis=(9, 10)), 'unsupported_profile')
    for key, val in (
            ('securityResult', {'integrityProtectionResult': 'not-performed',
                                'confidentialityProtectionResult': 'not-performed'}),
            ('qosFlowFailedToSetupList', [{'qosFlowIdentifier': 10, 'cause': ('nas', 'normal-release')}]),
            ('additionalDLQosFlowPerTNLInformation', [{'qosFlowPerTNLInformation': response()['dLQosFlowPerTNLInformation']}])):
        value = response(); value[key] = val
        add('setup_response', 'optional-'+key, value, 'unsupported_profile')
    mapped = response()
    mapped['dLQosFlowPerTNLInformation']['associatedQosFlowList'][0]['qosFlowMappingIndication'] = 'ul'
    add('setup_response', 'mapping-indication', mapped, 'unsupported_profile')
    for root, number, typename, payload in (
            ('setup_response', 193, 'QosFlowPerTNLInformation', response()['dLQosFlowPerTNLInformation']),
            ('release_command', 397, 'QosFlowAdditionalInfoListRelCom', [{'qosFlowIdentifier':9}]),
            ('release_response', 397, 'QosFlowAdditionalInfoListRelRes', [{'qosFlowIdentifier':9}])):
        value = response() if root=='setup_response' else (
            {'cause':('nas','normal-release')} if root=='release_command' else {})
        entry = {'id':number, 'criticality':'ignore', 'extensionValue':(typename, payload)}
        value['iE-Extensions'] = [entry]
        add(root, 'known-extension', deepcopy(value), 'unsupported_profile')
        value['iE-Extensions'][0]['criticality'] = 'reject'
        add(root, 'known-extension-criticality', deepcopy(value), 'invalid_transfer')
        value['iE-Extensions'][0]['criticality'] = 'ignore'
        value['iE-Extensions'].append(deepcopy(entry))
        add(root, 'known-extension-duplicate', value, 'invalid_transfer')
    return rows
