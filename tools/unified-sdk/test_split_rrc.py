#!/usr/bin/env python3
"""Small compiler/link and layout-seal regressions; no full SDK build."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from split_rrc import body_end, split_header, split_roots, reseal_layout

REPO=Path(__file__).resolve().parents[2]
HEADER='''#pragma once
#include <nrforge/rrc/runtime.hpp>
namespace fixture {
struct T0 {bool value=false;bool operator==(const T0&)const=default;};
inline ::nrforge::uper::Result<void> put0(::nrforge::uper::Writer& stream,const T0& v){return stream.write_bits(v.value,1);}
inline ::nrforge::uper::Result<T0> get0(::nrforge::uper::Reader& stream){auto r=stream.read_bits(1);if(!r)return ::nrforge::uper::Result<T0>::failure(r.error());return ::nrforge::uper::Result<T0>::success(T0{r.value()!=0});}
using Root=T0;
struct Decoded {Root value;std::size_t semantic_bits;};
inline ::nrforge::uper::Result<::nrforge::uper::CompleteEncoding> encode_unchecked(const Root& value,::nrforge::uper::Limits limits={},::nrforge::uper::Completion completion=::nrforge::uper::Completion::octets){::nrforge::uper::Writer w(limits);auto r=put0(w,value);if(!r)return ::nrforge::uper::Result<::nrforge::uper::CompleteEncoding>::failure(r.error());return w.finish(completion);}
inline ::nrforge::uper::Result<Decoded> decode_unchecked(std::span<const std::byte> bytes,::nrforge::uper::Limits limits={},std::optional<std::size_t> logical_bits={},::nrforge::uper::Completion completion=::nrforge::uper::Completion::octets){auto m=::nrforge::uper::Reader::make(bytes,limits,logical_bits);if(!m)return ::nrforge::uper::Result<Decoded>::failure(m.error());auto r=std::move(m).value();auto value=get0(r);if(!value)return ::nrforge::uper::Result<Decoded>::failure(value.error());auto n=r.cursor_bit();auto end=r.finish(completion);if(!end)return ::nrforge::uper::Result<Decoded>::failure(end.error());return ::nrforge::uper::Result<Decoded>::success(Decoded{std::move(value).value(),n});}
} // namespace
'''
MAIN='''#include <nrforge/rrc/root.hpp>
#include <iostream>
int main(){for(bool b:{false,true}){auto e=fixture::encode_unchecked({b});if(!e)return 1;auto d=fixture::decode_unchecked(e.value().octets);if(!d||d.value().value.value!=b||d.value().semantic_bits!=1)return 2;std::cout<<int(e.value().octets[0])<<":"<<e.value().semantic_bits<<"\\n";}std::byte bad[]{std::byte{0x81}};if(fixture::decode_unchecked(bad))return 3;return 0;}
'''


class SplitTests(unittest.TestCase):
    def test_bodies_preserved_and_defaults_declarations_only(self):
        header,bodies=split_header(HEADER)
        self.assertEqual(len(bodies),4)
        self.assertIn('limits={}',header)
        self.assertNotIn('limits={}',bodies[2])
        for body in bodies:
            self.assertIn(body[body.index('{'):],HEADER)
        self.assertNotIn('inline ::nrforge::uper::Result<',header)
        with self.assertRaises(ValueError):split_header(HEADER.replace('put0(', 'put9('))
        with self.assertRaises(ValueError):split_header(HEADER.replace('limits={}', 'limits=unknown'))

    def test_braces_ignore_strings_and_comments(self):
        text='{"}"; /* } */ [](){return "{";}; // }\n} trailing'
        self.assertEqual(text[:body_end(text,0)],text.removesuffix(' trailing'))
        with self.assertRaises(ValueError):body_end('{',0)

    def test_small_native_original_vs_partitioned(self):
        compiler=shutil.which('g++')
        if not compiler:self.skipTest('g++ unavailable')
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);inc=root/'include/nrforge/rrc';inc.mkdir(parents=True)
            shutil.copyfile(REPO/'libuper/runtime.hpp',inc/'runtime.hpp')
            header=inc/'root.hpp';header.write_text(HEADER)
            (root/'main.cpp').write_text(MAIN)
            def build(name,extra):
                argv=[compiler,'-std=c++20','-O0','-g0','-Wall','-Wextra','-Werror','-I'+str(root/'include'),str(root/'main.cpp'),*map(str,extra),str(REPO/'libuper/runtime.cpp'),'-o',str(root/name)]
                subprocess.run(argv,check=True,capture_output=True,timeout=120)
                return subprocess.check_output([root/name],timeout=10)
            expected=build('inline',[])
            new,bodies=split_header(HEADER);header.write_text(new)
            units=[]
            for i,body in enumerate(bodies):
                path=root/f'unit{i}.cpp';path.write_text('#include <nrforge/rrc/root.hpp>\nnamespace fixture {\n'+body+'\n}\n');units.append(path)
            self.assertEqual(build('split',units),expected)
            self.assertEqual(expected,b'0:1\n128:1\n')

    def test_six_roots_and_historical_headers_untouched(self):
        with tempfile.TemporaryDirectory() as directory:
            g=Path(directory);(g/'include').mkdir()
            entries=[]
            for i in range(91):
                path=g/'include'/f'r{i:03}.hpp';path.write_text(HEADER)
                entries.append(dict(index=i,header=path.name,namespace='fixture'))
            before=[(g/'include'/e['header']).read_bytes() for e in entries[:85]]
            sources,details=split_roots(g,entries,functions_per_unit=1)
            self.assertEqual(len(sources),24);self.assertEqual(len(details),6)
            self.assertEqual(before,[(g/'include'/e['header']).read_bytes() for e in entries[:85]])
            self.assertEqual(sum(len(x['units']) for x in details),24)

    def test_resealed_layout_verifies_and_tamper_fails(self):
        cmake=shutil.which('cmake')
        if not cmake:self.skipTest('cmake unavailable')
        with tempfile.TemporaryDirectory() as directory:
            g=Path(directory);inc=g/'include/nrforge/rrc';inc.mkdir(parents=True)
            entries=[]
            for i in range(85,91):
                path=inc/f'r{i:03}.hpp';path.write_text(HEADER)
                entries.append(dict(index=i,header='nrforge/rrc/'+path.name,namespace='fixture'))
            old='a'*64
            (inc/'sdk_version.hpp').write_text(old)
            (g/'sdk_identity.cpp').write_text(old)
            (g/'sdk_link_check.cpp').write_text('int main(){}')
            (g/'manifest.json').write_text(json.dumps(dict(roots=entries)))
            provenance=dict(identity=dict(version='0.3.0',fingerprint=old,runtime_sha256='b'*64,schema_sha256='c'*64,source_revision='d'*40),source_inputs={},source_authority={})
            (g/'sdk-provenance.json').write_text(json.dumps(provenance))
            (g/'CMakeLists.txt').write_text('add_library(nrforge_rrc STATIC\n)\nnrforge_package_rrc_sdk(nrforge_rrc)\n')
            layout=reseal_layout(g,'e'*40)
            self.assertEqual(layout['producer_revision'],'e'*40)
            revised=json.loads((g/'sdk-provenance.json').read_text())
            self.assertNotEqual(revised['identity']['fingerprint'],old)
            self.assertEqual(revised['identity']['source_revision'],'d'*40)
            argv=[cmake,'-DNRFORGE_SOURCE_ROOT='+str(REPO),'-DNRFORGE_GENERATED_ROOT='+str(g),'-P',str(REPO/'cmake/VerifyNgapSdk.cmake')]
            subprocess.run(argv,check=True,capture_output=True,timeout=10)
            (g/layout['roots'][0]['units'][0]).write_text('tampered')
            self.assertNotEqual(subprocess.run(argv,capture_output=True,timeout=10).returncode,0)


if __name__=='__main__':unittest.main()
