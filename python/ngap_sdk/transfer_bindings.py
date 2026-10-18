"""Render converters for the four installed independent root APIs."""
import json
import re


def generate_transfers(texts, receipt, rendered, parse_header, render):
    metadata = {}
    header = ['#pragma once', '#include "conversion.hpp"', 'namespace nrforge::python_sdk {']
    dispatch = ['#include "transfer_bindings.hpp"', 'namespace nrforge::python_sdk {']
    for stem, row in sorted(receipt['transfer_roots'].items()):
        namespace = 'nrforge::ngap::transfers::'+stem
        cpp_type = 'NgapIEsPduSessionResource'+row['message'][len('PDUSessionResource'):]
        if row['namespace'] != namespace or row['cpp_type'].lstrip(':') != namespace+'::'+cpp_type:
            raise ValueError('Transfer root type identity mismatch')
        codec = texts[f'transfers/{stem}/root_codec.hpp']
        matches = re.findall(r'inline .*?\b(encode_\w+)\(const ([\w:]+)& v', codec)
        if (row['encode'], row['cpp_type']) not in matches or row['decode'] != 'decode_'+row['encode'][7:]:
            raise ValueError('Transfer codec entry identity mismatch')
        declarations = parse_header(texts[f'transfers/{stem}/root_types.hpp'], namespace)
        if cpp_type not in {d['name'] for d in declarations}:
            raise ValueError('Missing Transfer root declaration')
        metadata[row['message']] = dict(body=cpp_type, declarations=declarations, **row)
        rendered['transfer_'+stem+'.cpp'] = render(stem, declarations, root=row)
        header += [f'py::bytes transfer_encode_{stem}(py::handle, const ::nrforge::aper::Limits&, ConversionBudget&);',
                   f'py::object transfer_decode_{stem}(const ::std::vector<::std::byte>&, const ::nrforge::aper::Limits&, ConversionBudget&);']
    header += ['py::bytes encode_transfer_body(const std::string&, py::handle, const ::nrforge::aper::Limits&, ConversionBudget&);',
               'py::object decode_transfer_body(const std::string&, const ::std::vector<::std::byte>&, const ::nrforge::aper::Limits&, ConversionBudget&);',
               'py::dict transfer_schema();', '}']
    dispatch += ['py::bytes encode_transfer_body(const std::string& name, py::handle body, const ::nrforge::aper::Limits& limits, ConversionBudget& budget) {']
    for stem, row in sorted(receipt['transfer_roots'].items()):
        dispatch.append(f'    if(name == "{row["message"]}") return transfer_encode_{stem}(body, limits, budget);')
    dispatch += ['    throw py::value_error("unknown Transfer root");', '}',
        'py::object decode_transfer_body(const std::string& name, const ::std::vector<::std::byte>& bytes, const ::nrforge::aper::Limits& limits, ConversionBudget& budget) {']
    for stem, row in sorted(receipt['transfer_roots'].items()):
        dispatch.append(f'    if(name == "{row["message"]}") return transfer_decode_{stem}(bytes, limits, budget);')
    dispatch += ['    throw py::value_error("unknown Transfer root");', '}', 'py::dict transfer_schema() {',
        '    return py::module_::import("json").attr("loads")('+json.dumps(json.dumps(metadata, separators=(',', ':')))+').cast<py::dict>();', '}', '}']
    rendered['transfer_bindings.hpp'] = '\n'.join(header)+'\n'
    rendered['transfer_dispatch.cpp'] = '\n'.join(dispatch)+'\n'
    rendered['sources.cmake'] = rendered['sources.cmake'].replace('\n)\n', '  "${CMAKE_CURRENT_LIST_DIR}/transfer_dispatch.cpp"\n)\n')
    return metadata
