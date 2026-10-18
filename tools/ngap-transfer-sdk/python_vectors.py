"""Project oracle semantic values into the documented typed Python containers.

Uses public declaration/mapping metadata for field names, never native decode.
Wire bytes are produced independently by pycrate from the frozen ASN.1 modules.
"""
import re


def project(bridge, desc, value, typ):
    kind = desc.TYPE
    bare = bridge.bare(typ)
    if kind in ('INTEGER', 'BOOLEAN', 'PrintableString', 'VisibleString', 'UTF8String', 'OCTET STRING'):
        return value
    if kind == 'NULL':
        return None
    if kind == 'BIT STRING':
        number, width = value
        return dict(octets=(number << ((-width) % 8)).to_bytes((width+7)//8, 'big'), bit_count=width)
    if kind == 'ENUMERATED':
        number = desc._cont[value]
        entries = re.findall(r'(\w+)\s*=\s*(?:INT64_C\((-?\d+)\)|\(-INT64_C\((\d+)\)\))', bridge.structs[bare])
        return next(name for name, signed, negative in entries if int(signed or '-'+negative) == number)
    if kind == 'SEQUENCE OF':
        vector_type = next(t for t, name in bridge.fields(typ) if name == 'elements')
        element = vector_type[len('::std::vector<'):-1]
        return dict(elements=[project(bridge, desc._cont, v, element) for v in value])
    if kind == 'CHOICE':
        label, payload = value
        ordinal = list(desc._cont).index(label)
        from semantic_bridge import split_args
        alias = bridge.aliases[bare]
        alternative = split_args(alias[alias.index('<')+1:-1])[ordinal]
        payload_type = bridge.fields(alternative)[0][0]
        return dict(type=bridge.bare(alternative), value=dict(value=project(bridge, desc._cont[label], payload, payload_type)))
    if kind == 'SEQUENCE':
        fields = bridge.fields(typ)
        if len(fields) != len(desc._cont) or set(value)-set(desc._cont):
            raise ValueError('Unrepresented oracle SEQUENCE fields')
        result = {}
        for (name, child), (field_type, member) in zip(desc._cont.items(), fields):
            if field_type.startswith('::std::optional<'):
                if name not in value:
                    result[member] = None
                    continue
                field_type = field_type[len('::std::optional<'):-1]
            if child.TYPE != 'OPEN_TYPE':
                result[member] = project(bridge, child, value[name], field_type)
                continue
            label, payload = value[name]
            mapping = bridge.maps[bare+'_aper']
            if label.startswith('_unk_'):
                wrapper = re.search(r'using unknown_type = (.*?);', mapping)[1]
                result[member] = dict(type=bridge.bare(wrapper), value=dict(payload=payload))
            else:
                table = re.search(r'rows\{\{(.*?)\}\};', mapping, re.S)[1]
                ids = [int(n) for n in re.findall(r'\{(\d+),', table)]
                index = ids.index(value['id'])
                wrapper = re.search(r'using wrapper_'+str(index)+r' = (.*?);', mapping)[1]
                payload_type = re.search(r'using payload_type_'+str(index)+r' = (.*?);', mapping)[1]
                rows = child._const_tab._val
                native = next(row[child._const_tab_id] for row in list(rows.root)+list(rows.ext or []) if row['id'] == value['id'])
                result[member] = dict(type=bridge.bare(wrapper), value=dict(value=project(bridge, native, payload, payload_type)))
        if 'sequence_extensions' in bridge.structs[bare]:
            result['sequence_extensions'] = dict(received_bitmap_bit_count=0, unknown_additions=[])
        return result
    raise ValueError('Unsupported oracle projection kind: '+kind)
