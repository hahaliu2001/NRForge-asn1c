"""N10 semantic models reused for N11; no ASN.1 source or wire rewriting."""

def build_body_cases(ies):
    branches = {'radio_network': ('radioNetwork', 'CauseRadioNetwork'), 'transport': ('transport', 'CauseTransport'), 'nas': ('nas', 'CauseNas'), 'protocol': ('protocol', 'CauseProtocol'), 'misc': ('misc', 'CauseMisc')}
    policies = ('reject', 'ignore', 'notify')

    def sequence(text):
        if text == '-': return {}
        width, rows = text.split('/', 1)
        # Native unknown SEQUENCE suffix handling is inconsistent. Do not normalize it.
        raise ValueError('Opaque sequence sidecars use separate literal rejection evidence')

    def to_native(model):
        seq, entries = model.split('|', 1)
        value = {'protocolIEs': []}; value.update(sequence(seq))
        for record in entries.split(';'):
            if not record: continue
            fields = record.split(':'); ident, criticality = int(fields[0]), int(fields[1]); kind = fields[2]
            if kind == 'pair':
                pair = {'aMF-UE-NGAP-ID': int(fields[3]), 'rAN-UE-NGAP-ID': int(fields[4])}
                if fields[5] != '-':
                    pair['iE-Extensions'] = [{'id': int(r.split(',')[0]), 'criticality': policies[int(r.split(',')[1])], 'extensionValue': ('_unk_004', bytes.fromhex(r.split(',')[2]))} for r in fields[5].split('+')]
                pair.update(sequence(fields[6])); payload = ('UE-NGAP-IDs', ('uE-NGAP-ID-pair', pair))
            elif kind == 'amf': payload = ('UE-NGAP-IDs', ('aMF-UE-NGAP-ID', int(fields[3])))
            elif kind in ('ue_ext', 'cause_ext'):
                extension = {'id': int(fields[3]), 'criticality': policies[int(fields[4])], 'value': ('_unk_004', bytes.fromhex(fields[5]))}
                payload = ('UE-NGAP-IDs' if kind == 'ue_ext' else 'Cause', ('choice-Extensions', extension))
            elif kind == 'cause':
                branch, typename = branches[fields[3]]; enum = getattr(ies, typename)
                label = '_ext_' + fields[5] if fields[4] == 'x' else next(k for k, n in enum._cont.items() if n == int(fields[5]))
                payload = ('Cause', (branch, label))
            elif kind == 'unknown': payload = ('_unk_004', bytes.fromhex(fields[3]))
            else: raise ValueError(kind)
            value['protocolIEs'].append({'id': ident, 'criticality': policies[criticality], 'value': payload})
        return value

    cases = []
    base_ids = '114:0:pair:1:2:-:-'
    base_cause = '15:1:cause:radio_network:k:0'
    def add(name, records, opaque=False): cases.append((name, '-|' + records, opaque))
    for key, (_, typename) in branches.items():
        enum = getattr(ies, typename)
        for label, number in enum._cont.items(): add('known-'+typename+'-'+label, base_ids+f';15:1:cause:{key}:k:{number}')
        for index in sorted(set((len(enum._ext or []),63,64,255,256))): add('unknown-'+typename+'-'+str(index), base_ids+f';15:1:cause:{key}:x:{index}')
    amfs = (0,1,255,256,65535,65536,16777215,16777216,4294967295,4294967296,1099511627775)
    rans = (0,1,255,256,65535,65536,16777215,16777216,4294967295)
    for amf in amfs:
        add('amf-'+str(amf),f'114:0:amf:{amf};'+base_cause)
        for ran in rans: add(f'pair-{amf}-{ran}',f'114:0:pair:{amf}:{ran}:-:-;'+base_cause)
    for left in range(3):
        for right in range(3): add(f'received-criticality-{left}-{right}',f'114:{left}:pair:1:2:-:-;15:{right}:cause:nas:k:0')
    for name, entries in [('empty',''),('missing-cause',base_ids),('missing-ids',base_cause),('reordered',base_cause+';'+base_ids),('duplicate-ids',base_ids+';'+base_ids+';'+base_cause),('duplicate-cause',base_ids+';'+base_cause+';'+base_cause)]: add(name,entries)
    for ident in (0,65535):
        for crit in range(3):
            for size in (1,3,127,128,16384,65536):
                payload = bytes((i*17+128)%256 for i in range(size)).hex()
                add(f'main-unknown-{ident}-{crit}-{size}',base_ids+';'+base_cause+f';{ident}:{crit}:unknown:{payload}',True)
                add(f'ue-choice-unknown-{ident}-{crit}-{size}',f'114:0:ue_ext:{ident}:{crit}:{payload};'+base_cause,True)
                add(f'cause-choice-unknown-{ident}-{crit}-{size}',base_ids+f';15:1:cause_ext:{ident}:{crit}:{payload}',True)
                add(f'pair-extension-unknown-{ident}-{crit}-{size}',f'114:0:pair:1:2:{ident},{crit},{payload}:-;'+base_cause,True)
    return cases, to_native
